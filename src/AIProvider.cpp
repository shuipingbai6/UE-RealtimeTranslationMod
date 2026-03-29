#include <AIProvider.hpp>
#include <NetworkClient.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

#include <sstream>
#include <thread>

namespace RC::RealtimeTranslation
{
    // JSON辅助函数
    static auto JsonEscape(const std::wstring& s) -> std::wstring
    {
        std::wstring result;
        result.reserve(s.size());
        for (auto c : s)
        {
            switch (c)
            {
                case L'"': result += L"\\\""; break;
                case L'\\': result += L"\\\\"; break;
                case L'\n': result += L"\\n"; break;
                case L'\r': result += L"\\r"; break;
                case L'\t': result += L"\\t"; break;
                default: result += c; break;
            }
        }
        return result;
    }

    static auto JsonUnescape(const std::wstring& s) -> std::wstring
    {
        std::wstring result;
        result.reserve(s.size());
        bool escape = false;
        for (auto c : s)
        {
            if (escape)
            {
                switch (c)
                {
                    case L'"': result += L'"'; break;
                    case L'\\': result += L'\\'; break;
                    case L'n': result += L'\n'; break;
                    case L'r': result += L'\r'; break;
                    case L't': result += L'\t'; break;
                    default: result += c; break;
                }
                escape = false;
            }
            else if (c == L'\\')
            {
                escape = true;
            }
            else
            {
                result += c;
            }
        }
        return result;
    }

    // 从JSON响应中提取内容
    static auto ExtractContentFromResponse(const std::wstring& json) -> std::wstring
    {
        // OpenAI响应格式: {"choices":[{"message":{"content":"..."}}]}
        // 注意：某些响应中 delta.content 可能为空，需要优先查找 message.content

        // 首先尝试查找 "message" 对象里的 "content"
        size_t messagePos = json.find(L"\"message\"");
        if (messagePos != std::wstring::npos)
        {
            // 在 message 对象内查找 content
            size_t messageStart = json.find(L'{', messagePos);
            if (messageStart != std::wstring::npos)
            {
                size_t messageEnd = messageStart;
                int braceCount = 1;
                while (messageEnd < json.size() && braceCount > 0)
                {
                    if (json[messageEnd] == L'{') braceCount++;
                    else if (json[messageEnd] == L'}') braceCount--;
                    if (braceCount > 0) messageEnd++;
                }
                
                std::wstring messageContent = json.substr(messageStart, messageEnd - messageStart);
                size_t contentPos = messageContent.find(L"\"content\"");
                if (contentPos != std::wstring::npos)
                {
                    size_t colonPos = messageContent.find(L':', contentPos);
                    if (colonPos != std::wstring::npos)
                    {
                        size_t startPos = colonPos + 1;
                        while (startPos < messageContent.size() && 
                               (messageContent[startPos] == L' ' || messageContent[startPos] == L'\t' ||
                                messageContent[startPos] == L'\n' || messageContent[startPos] == L'\r'))
                            startPos++;
                        
                        if (startPos < messageContent.size() && messageContent[startPos] == L'"')
                        {
                            startPos++;
                            size_t endPos = startPos;
                            bool escape = false;
                            while (endPos < messageContent.size())
                            {
                                if (escape) escape = false;
                                else if (messageContent[endPos] == L'\\') escape = true;
                                else if (messageContent[endPos] == L'"') break;
                                endPos++;
                            }
                            
                            std::wstring result = JsonUnescape(messageContent.substr(startPos, endPos - startPos));
                            if (!result.empty())
                            {
                                return result;
                            }
                        }
                    }
                }
            }
        }

        // 回退：查找任意 "content" 字段（跳过空值）
        size_t searchPos = 0;
        while (searchPos < json.size())
        {
            size_t contentPos = json.find(L"\"content\"", searchPos);
            if (contentPos == std::wstring::npos)
            {
                break;
            }

            size_t colonPos = json.find(L':', contentPos);
            if (colonPos == std::wstring::npos)
            {
                searchPos = contentPos + 1;
                continue;
            }

            size_t startPos = colonPos + 1;
            while (startPos < json.size() && (json[startPos] == L' ' || json[startPos] == L'\t' ||
                   json[startPos] == L'\n' || json[startPos] == L'\r'))
                startPos++;

            if (startPos >= json.size() || json[startPos] != L'"')
            {
                searchPos = startPos;
                continue;
            }

            startPos++;
            size_t endPos = startPos;
            bool escape = false;
            while (endPos < json.size())
            {
                if (escape) escape = false;
                else if (json[endPos] == L'\\') escape = true;
                else if (json[endPos] == L'"') break;
                endPos++;
            }

            std::wstring result = JsonUnescape(json.substr(startPos, endPos - startPos));
            if (!result.empty())
            {
                return result;
            }
            
            searchPos = endPos + 1;
        }

        // 最后尝试查找 "text" 字段
        size_t textPos = json.find(L"\"text\"");
        if (textPos != std::wstring::npos)
        {
            size_t colonPos = json.find(L':', textPos);
            if (colonPos != std::wstring::npos)
            {
                size_t startPos = colonPos + 1;
                while (startPos < json.size() && (json[startPos] == L' ' || json[startPos] == L'\t' ||
                       json[startPos] == L'\n' || json[startPos] == L'\r'))
                    startPos++;

                if (startPos < json.size() && json[startPos] == L'"')
                {
                    startPos++;
                    size_t endPos = startPos;
                    bool escape = false;
                    while (endPos < json.size())
                    {
                        if (escape) escape = false;
                        else if (json[endPos] == L'\\') escape = true;
                        else if (json[endPos] == L'"') break;
                        endPos++;
                    }
                    return JsonUnescape(json.substr(startPos, endPos - startPos));
                }
            }
        }

        return L"";
    }

    AIProvider& AIProvider::Instance()
    {
        static AIProvider instance;
        return instance;
    }

    auto AIProvider::Initialize(const AIProviderConfig& config) -> bool
    {
        std::unique_lock lock(m_mutex);
        m_config = config;
        m_initialized = true;
        m_lastRequestTime = std::chrono::steady_clock::time_point{};

        Output::send<LogLevel::Normal>(STR("[AIProvider] Initialized with endpoint: {}\n"), m_config.ApiEndpoint);
        return true;
    }

    auto AIProvider::WaitForRateLimit() -> void
    {
        std::lock_guard<std::mutex> lock(m_rateLimitMutex);

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastRequestTime);

        int minInterval = m_config.RequestIntervalMs > 0 ? m_config.RequestIntervalMs : 500;

        if (elapsed.count() < minInterval)
        {
            auto waitTime = minInterval - elapsed.count();
            std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
        }

        m_lastRequestTime = std::chrono::steady_clock::now();
    }

    auto AIProvider::Translate(
        const std::wstring& text,
        const std::wstring& sourceLang,
        const std::wstring& targetLang
    ) -> TranslationResult
    {
        auto startTime = std::chrono::steady_clock::now();

        TranslationResult result;
        result.OriginalText = text;

        if (!IsConfigured())
        {
            result.ErrorMessage = L"AI provider not configured";
            return result;
        }

        auto requestBody = BuildTranslationRequest(text, sourceLang, targetLang);
        auto authorization = L"Bearer " + m_config.ApiKey;

        int maxRetries = m_config.MaxRetries > 0 ? m_config.MaxRetries : 3;

        for (int retry = 0; retry <= maxRetries; ++retry)
        {
            WaitForRateLimit();

            auto httpResponse = NetworkClient::Instance().Post(
                m_config.ApiEndpoint,
                requestBody,
                L"application/json",
                authorization,
                m_config.TimeoutMs
            );

            auto endTime = std::chrono::steady_clock::now();
            result.Duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            if (httpResponse.Success)
            {
                auto translatedText = ParseTranslationResponse(httpResponse.ResponseBody);
                if (!translatedText.empty())
                {
                    result.TranslatedText = translatedText;
                    result.Success = true;
                    return result;
                }
                result.ErrorMessage = L"Failed to parse translation response";
                Output::send<LogLevel::Warning>(STR("[AIProvider] Response: {}\n"), httpResponse.ResponseBody);
            }
            else
            {
                result.ErrorMessage = httpResponse.ErrorMessage + L" (HTTP " + std::to_wstring(httpResponse.StatusCode) + L")";

                if (httpResponse.StatusCode == 429)
                {
                    int baseDelay = 1000;
                    int delay = baseDelay * (1 << retry);
                    if (delay > 30000) delay = 30000;

                    Output::send<LogLevel::Warning>(STR("[AIProvider] Rate limited (429), waiting {}ms before retry {}/{}\n"),
                        delay, retry + 1, maxRetries);

                    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                    continue;
                }
                else if (httpResponse.StatusCode >= 500 || httpResponse.StatusCode == 0)
                {
                    int delay = 500 * (retry + 1);
                    Output::send<LogLevel::Warning>(STR("[AIProvider] Server error, retrying in {}ms ({}/{})\n"),
                        delay, retry + 1, maxRetries);
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                    continue;
                }
                else
                {
                    break;
                }
            }

            if (retry < maxRetries)
            {
                int delay = 300 * (retry + 1);
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            }
        }

        return result;
    }

    auto AIProvider::TranslateBatch(
        const std::vector<std::wstring>& texts,
        const std::wstring& sourceLang,
        const std::wstring& targetLang
    ) -> std::vector<TranslationResult>
    {
        std::vector<TranslationResult> results;
        results.reserve(texts.size());

        for (const auto& text : texts)
        {
            results.push_back(Translate(text, sourceLang, targetLang));
        }

        return results;
    }

    auto AIProvider::UpdateConfig(const AIProviderConfig& config) -> void
    {
        std::unique_lock lock(m_mutex);
        m_config = config;
    }

    auto AIProvider::IsConfigured() const -> bool
    {
        std::shared_lock lock(m_mutex);
        return !m_config.ApiEndpoint.empty() &&
               !m_config.ApiKey.empty() &&
               !m_config.Model.empty();
    }

    auto AIProvider::TestConnection() -> std::pair<bool, std::wstring>
    {
        if (!IsConfigured())
        {
            return {false, L"AI provider not configured"};
        }

        // 发送一个简单的测试请求
        auto result = Translate(L"Hello", L"en", L"zh-CN");
        return {result.Success, result.Success ? L"Connection successful" : result.ErrorMessage};
    }

    auto AIProvider::BuildTranslationRequest(
        const std::wstring& text,
        const std::wstring& sourceLang,
        const std::wstring& targetLang
    ) -> std::wstring
    {
        // 构建OpenAI兼容的请求格式
        auto prompt = BuildTranslationPrompt(text, sourceLang, targetLang);

        std::wstringstream json;
        json << L"{\n";
        json << L"  \"model\": \"" << JsonEscape(m_config.Model) << L"\",\n";
        json << L"  \"messages\": [\n";
        json << L"    {\n";
        json << L"      \"role\": \"system\",\n";
        json << L"      \"content\": \"You are a professional game translator. Translate the given text accurately and naturally. Only output the translation, no explanations.\"\n";
        json << L"    },\n";
        json << L"    {\n";
        json << L"      \"role\": \"user\",\n";
        json << L"      \"content\": \"" << JsonEscape(prompt) << L"\"\n";
        json << L"    }\n";
        json << L"  ],\n";
        json << L"  \"temperature\": 0.3,\n";
        json << L"  \"max_tokens\": 1024\n";
        json << L"}";

        return json.str();
    }

    auto AIProvider::ParseTranslationResponse(const std::wstring& response) -> std::wstring
    {
        return ExtractContentFromResponse(response);
    }

    auto AIProvider::BuildTranslationPrompt(
        const std::wstring& text,
        const std::wstring& sourceLang,
        const std::wstring& targetLang
    ) -> std::wstring
    {
        std::wstringstream prompt;

        // 构建翻译提示
        if (sourceLang == L"auto")
        {
            prompt << L"Translate the following text to " << targetLang << L". ";
            prompt << L"Preserve any formatting, special characters, and placeholders like {0}, %s, etc. ";
            prompt << L"Only output the translation, nothing else.\n\n";
            prompt << L"Text to translate:\n" << text;
        }
        else
        {
            prompt << L"Translate the following text from " << sourceLang << L" to " << targetLang << L". ";
            prompt << L"Preserve any formatting, special characters, and placeholders like {0}, %s, etc. ";
            prompt << L"Only output the translation, nothing else.\n\n";
            prompt << L"Text to translate:\n" << text;
        }

        return prompt.str();
    }
} // namespace RC::RealtimeTranslation
