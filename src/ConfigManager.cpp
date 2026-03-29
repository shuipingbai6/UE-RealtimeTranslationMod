#include <ConfigManager.hpp>
#include <VocabularyCache.hpp>
#include <DynamicOutput/DynamicOutput.hpp>
#include <File/File.hpp>

#include <fstream>
#include <sstream>
#include <filesystem>

// 简单的JSON解析（不依赖外部库）
// 注意：UE4SS内置glaze库，但我们这里使用简单的字符串解析以避免复杂性

namespace RC::RealtimeTranslation
{
    // 简单的JSON字符串转义处理
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

    // 简单的JSON字符串提取
    static auto ExtractJsonString(const std::wstring& json, const std::wstring& key) -> std::wstring
    {
        std::wstring searchKey = L"\"" + key + L"\"";
        size_t pos = json.find(searchKey);
        if (pos == std::wstring::npos) return L"";

        pos = json.find(L':', pos);
        if (pos == std::wstring::npos) return L"";

        // 跳过空白
        pos++;
        while (pos < json.size() && (json[pos] == L' ' || json[pos] == L'\t' || json[pos] == L'\n' || json[pos] == L'\r'))
            pos++;

        if (pos >= json.size() || json[pos] != L'"') return L"";

        // 提取字符串值
        pos++;
        size_t start = pos;
        bool escape = false;
        while (pos < json.size())
        {
            if (escape)
            {
                escape = false;
            }
            else if (json[pos] == L'\\')
            {
                escape = true;
            }
            else if (json[pos] == L'"')
            {
                break;
            }
            pos++;
        }

        return JsonUnescape(json.substr(start, pos - start));
    }

    static auto ExtractJsonInt(const std::wstring& json, const std::wstring& key, int defaultValue = 0) -> int
    {
        std::wstring searchKey = L"\"" + key + L"\"";
        size_t pos = json.find(searchKey);
        if (pos == std::wstring::npos) return defaultValue;

        pos = json.find(L':', pos);
        if (pos == std::wstring::npos) return defaultValue;

        // 跳过空白
        pos++;
        while (pos < json.size() && (json[pos] == L' ' || json[pos] == L'\t' || json[pos] == L'\n' || json[pos] == L'\r'))
            pos++;

        // 提取数字
        std::wstring numStr;
        while (pos < json.size() && (json[pos] >= L'0' && json[pos] <= L'9'))
        {
            numStr += json[pos];
            pos++;
        }

        return numStr.empty() ? defaultValue : std::stoi(numStr);
    }

    static auto ExtractJsonBool(const std::wstring& json, const std::wstring& key, bool defaultValue = false) -> bool
    {
        std::wstring searchKey = L"\"" + key + L"\"";
        size_t pos = json.find(searchKey);
        if (pos == std::wstring::npos) return defaultValue;

        pos = json.find(L':', pos);
        if (pos == std::wstring::npos) return defaultValue;

        // 跳过空白
        pos++;
        while (pos < json.size() && (json[pos] == L' ' || json[pos] == L'\t' || json[pos] == L'\n' || json[pos] == L'\r'))
            pos++;

        if (pos + 3 < json.size() && json.substr(pos, 4) == L"true") return true;
        if (pos + 4 < json.size() && json.substr(pos, 5) == L"false") return false;
        return defaultValue;
    }

    ConfigManager& ConfigManager::Instance()
    {
        static ConfigManager instance;
        return instance;
    }

    auto ConfigManager::Initialize(const std::wstring& configPath) -> bool
    {
        m_configPath = configPath;

        std::ifstream file(configPath);
        if (!file.good())
        {
            file.close();
            std::unique_lock lock(m_mutex);
            if (!CreateDefaultConfigInternal())
            {
                Output::send<LogLevel::Warning>(STR("[ConfigManager] Failed to create default config file.\n"));
                m_initialized = true;
                return true;
            }
        }

        m_initialized = true;
        return LoadConfig();
    }

    auto ConfigManager::LoadConfig() -> bool
    {
        std::wstring configPathCopy;
        {
            std::shared_lock readLock(m_mutex);
            configPathCopy = m_configPath;
        }

        if (!std::filesystem::exists(configPathCopy))
        {
            return false;
        }

        std::wifstream file(configPathCopy);
        if (!file.is_open())
        {
            return false;
        }

        std::wstringstream buffer;
        buffer << file.rdbuf();
        std::wstring jsonContent = buffer.str();
        file.close();

        std::unique_lock writeLock(m_mutex);
        ParseConfigFromJson(jsonContent);

        Output::send<LogLevel::Normal>(STR("[ConfigManager] Config loaded from: {}\n"), m_configPath);
        return true;
    }

    auto ConfigManager::ParseConfigFromJson(const std::wstring& jsonContent) -> void
    {
        m_config.AIProvider.ApiEndpoint = ExtractJsonString(jsonContent, L"api_endpoint");
        m_config.AIProvider.ApiKey = ExtractJsonString(jsonContent, L"api_key");
        m_config.AIProvider.Model = ExtractJsonString(jsonContent, L"model");
        m_config.AIProvider.TimeoutMs = ExtractJsonInt(jsonContent, L"timeout_ms", 10000);
        m_config.AIProvider.MaxRetries = ExtractJsonInt(jsonContent, L"max_retries", 3);
        m_config.AIProvider.RequestIntervalMs = ExtractJsonInt(jsonContent, L"request_interval_ms", 500);
        m_config.AIProvider.MaxRequestsPerSecond = ExtractJsonInt(jsonContent, L"max_requests_per_second", 2);

        m_config.Translation.SourceLanguage = ExtractJsonString(jsonContent, L"source_language");
        if (m_config.Translation.SourceLanguage.empty())
            m_config.Translation.SourceLanguage = L"auto";
        m_config.Translation.TargetLanguage = ExtractJsonString(jsonContent, L"target_language");
        if (m_config.Translation.TargetLanguage.empty())
            m_config.Translation.TargetLanguage = L"zh-CN";

        m_config.HookFilter.MinTextLength = ExtractJsonInt(jsonContent, L"min_text_length", 2);

        m_config.Vocabulary.FilePath = ExtractJsonString(jsonContent, L"vocabulary_file");
        if (m_config.Vocabulary.FilePath.empty())
            m_config.Vocabulary.FilePath = L"vocabulary.json";
        m_config.Vocabulary.AutoSave = ExtractJsonBool(jsonContent, L"auto_save", true);
        m_config.Vocabulary.SaveIntervalMs = ExtractJsonInt(jsonContent, L"save_interval_ms", 5000);
    }

    auto ConfigManager::CreateDefaultConfigInternal() -> bool
    {
        m_config.AIProvider.ApiEndpoint = L"";
        m_config.AIProvider.ApiKey = L"";
        m_config.AIProvider.Model = L"gpt-4";
        m_config.AIProvider.TimeoutMs = 10000;
        m_config.AIProvider.MaxRetries = 3;
        m_config.AIProvider.RequestIntervalMs = 500;
        m_config.AIProvider.MaxRequestsPerSecond = 2;

        m_config.Translation.SourceLanguage = L"auto";
        m_config.Translation.TargetLanguage = L"zh-CN";

        m_config.HookFilter.MinTextLength = 2;

        m_config.Vocabulary.FilePath = L"vocabulary.json";
        m_config.Vocabulary.AutoSave = true;
        m_config.Vocabulary.SaveIntervalMs = 5000;

        return SaveConfigInternal();
    }

    auto ConfigManager::SaveConfigInternal() -> bool
    {
        ModConfig configCopy = m_config;
        std::wstring pathCopy = m_configPath;

        std::wstringstream json;
        json << L"{\n";
        json << L"  \"ai_provider\": {\n";
        json << L"    \"api_endpoint\": \"" << JsonEscape(configCopy.AIProvider.ApiEndpoint) << L"\",\n";
        json << L"    \"api_key\": \"" << JsonEscape(configCopy.AIProvider.ApiKey) << L"\",\n";
        json << L"    \"model\": \"" << JsonEscape(configCopy.AIProvider.Model) << L"\",\n";
        json << L"    \"timeout_ms\": " << configCopy.AIProvider.TimeoutMs << L",\n";
        json << L"    \"max_retries\": " << configCopy.AIProvider.MaxRetries << L",\n";
        json << L"    \"request_interval_ms\": " << configCopy.AIProvider.RequestIntervalMs << L",\n";
        json << L"    \"max_requests_per_second\": " << configCopy.AIProvider.MaxRequestsPerSecond << L"\n";
        json << L"  },\n";
        json << L"  \"translation\": {\n";
        json << L"    \"source_language\": \"" << configCopy.Translation.SourceLanguage << L"\",\n";
        json << L"    \"target_language\": \"" << configCopy.Translation.TargetLanguage << L"\"\n";
        json << L"  },\n";
        json << L"  \"hooks\": {\n";
        json << L"    \"min_text_length\": " << configCopy.HookFilter.MinTextLength << L"\n";
        json << L"  },\n";
        json << L"  \"vocabulary\": {\n";
        json << L"    \"file_path\": \"" << configCopy.Vocabulary.FilePath << L"\",\n";
        json << L"    \"auto_save\": " << (configCopy.Vocabulary.AutoSave ? L"true" : L"false") << L",\n";
        json << L"    \"save_interval_ms\": " << configCopy.Vocabulary.SaveIntervalMs << L"\n";
        json << L"  }\n";
        json << L"}\n";

        try
        {
            std::filesystem::path filePath(pathCopy);
            auto parentDir = filePath.parent_path();
            if (!parentDir.empty() && !std::filesystem::exists(parentDir))
            {
                std::filesystem::create_directories(parentDir);
            }

            std::wofstream file(filePath);
            if (!file.is_open())
            {
                Output::send<LogLevel::Error>(STR("[ConfigManager] Failed to open config file for writing: {}\n"), pathCopy);
                return false;
            }

            file << json.str();
            file.close();

            Output::send<LogLevel::Normal>(STR("[ConfigManager] Config saved to: {}\n"), pathCopy);
            return true;
        }
        catch (const std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("[ConfigManager] Exception while saving config: {}\n"), 
                to_wstring(std::string(e.what())));
            return false;
        }
    }

    auto ConfigManager::SaveConfig() -> bool
    {
        ModConfig configCopy;
        std::wstring pathCopy;
        {
            std::shared_lock lock(m_mutex);
            configCopy = m_config;
            pathCopy = m_configPath;
        }

        std::wstringstream json;
        json << L"{\n";
        json << L"  \"ai_provider\": {\n";
        json << L"    \"api_endpoint\": \"" << JsonEscape(configCopy.AIProvider.ApiEndpoint) << L"\",\n";
        json << L"    \"api_key\": \"" << JsonEscape(configCopy.AIProvider.ApiKey) << L"\",\n";
        json << L"    \"model\": \"" << JsonEscape(configCopy.AIProvider.Model) << L"\",\n";
        json << L"    \"timeout_ms\": " << configCopy.AIProvider.TimeoutMs << L",\n";
        json << L"    \"max_retries\": " << configCopy.AIProvider.MaxRetries << L",\n";
        json << L"    \"request_interval_ms\": " << configCopy.AIProvider.RequestIntervalMs << L",\n";
        json << L"    \"max_requests_per_second\": " << configCopy.AIProvider.MaxRequestsPerSecond << L"\n";
        json << L"  },\n";
        json << L"  \"translation\": {\n";
        json << L"    \"source_language\": \"" << configCopy.Translation.SourceLanguage << L"\",\n";
        json << L"    \"target_language\": \"" << configCopy.Translation.TargetLanguage << L"\"\n";
        json << L"  },\n";
        json << L"  \"hooks\": {\n";
        json << L"    \"min_text_length\": " << configCopy.HookFilter.MinTextLength << L"\n";
        json << L"  },\n";
        json << L"  \"vocabulary\": {\n";
        json << L"    \"file_path\": \"" << configCopy.Vocabulary.FilePath << L"\",\n";
        json << L"    \"auto_save\": " << (configCopy.Vocabulary.AutoSave ? L"true" : L"false") << L",\n";
        json << L"    \"save_interval_ms\": " << configCopy.Vocabulary.SaveIntervalMs << L"\n";
        json << L"  }\n";
        json << L"}\n";

        try
        {
            std::filesystem::path filePath(pathCopy);
            auto parentDir = filePath.parent_path();
            if (!parentDir.empty() && !std::filesystem::exists(parentDir))
            {
                std::filesystem::create_directories(parentDir);
            }

            std::wofstream file(filePath);
            if (!file.is_open())
            {
                Output::send<LogLevel::Error>(STR("[ConfigManager] Failed to open config file for writing: {}\n"), pathCopy);
                return false;
            }

            file << json.str();
            file.close();

            Output::send<LogLevel::Normal>(STR("[ConfigManager] Config saved to: {}\n"), pathCopy);
            return true;
        }
        catch (const std::exception& e)
        {
            Output::send<LogLevel::Error>(STR("[ConfigManager] Exception while saving config: {}\n"), 
                to_wstring(std::string(e.what())));
            return false;
        }
    }

    auto ConfigManager::UpdateConfig(const ModConfig& newConfig) -> void
    {
        {
            std::unique_lock lock(m_mutex);
            m_config = newConfig;
        }

        SaveConfig();
    }

    auto ConfigManager::ValidateConfig() const -> bool
    {
        std::shared_lock lock(m_mutex);

        // 检查API配置
        if (m_config.AIProvider.ApiEndpoint.empty())
        {
            Output::send<LogLevel::Warning>(STR("[ConfigManager] API endpoint is empty.\n"));
            return false;
        }

        if (m_config.AIProvider.ApiKey.empty())
        {
            Output::send<LogLevel::Warning>(STR("[ConfigManager] API key is empty.\n"));
            return false;
        }

        if (m_config.AIProvider.Model.empty())
        {
            Output::send<LogLevel::Warning>(STR("[ConfigManager] Model name is empty.\n"));
            return false;
        }

        return true;
    }

    auto ConfigManager::CreateDefaultConfig() -> bool
    {
        std::unique_lock lock(m_mutex);

        m_config.AIProvider.ApiEndpoint = L"";
        m_config.AIProvider.ApiKey = L"";
        m_config.AIProvider.Model = L"gpt-4";
        m_config.AIProvider.TimeoutMs = 10000;
        m_config.AIProvider.MaxRetries = 3;
        m_config.AIProvider.RequestIntervalMs = 500;
        m_config.AIProvider.MaxRequestsPerSecond = 2;

        m_config.Translation.SourceLanguage = L"auto";
        m_config.Translation.TargetLanguage = L"zh-CN";

        m_config.HookFilter.MinTextLength = 2;

        m_config.Vocabulary.FilePath = L"vocabulary.json";
        m_config.Vocabulary.AutoSave = true;
        m_config.Vocabulary.SaveIntervalMs = 5000;

        return SaveConfigInternal();
    }

    auto ConfigManager::SyncToSharedMemory() -> void
    {
        auto& shmMgr = SharedMemoryManager::Instance();
        if (!shmMgr.IsValid()) return;

        auto* shm = shmMgr.Get();
        if (!shm) return;

        std::shared_lock lock(m_mutex);
        shm->SetConfig(m_config.AIProvider, m_config.Translation);
    }

    auto ConfigManager::SyncFromSharedMemory() -> bool
    {
        auto& shmMgr = SharedMemoryManager::Instance();
        if (!shmMgr.IsValid()) return false;

        auto* shm = shmMgr.Get();
        if (!shm) return false;

        if (InterlockedCompareExchange(&shm->ConfigUpdated, 0, 1) == 1)
        {
            std::unique_lock lock(m_mutex);
            shm->GetConfig(m_config.AIProvider, m_config.Translation);
            return true;
        }

        return false;
    }

    auto ConfigManager::CheckSharedMemoryRequests() -> void
    {
        auto& shmMgr = SharedMemoryManager::Instance();
        if (!shmMgr.IsValid()) return;

        auto* shm = shmMgr.Get();
        if (!shm) return;

        if (InterlockedCompareExchange(&shm->RequestReloadConfig, 0, 1) == 1)
        {
            LoadConfig();
            SyncToSharedMemory();
        }

        if (InterlockedCompareExchange(&shm->RequestSaveVocab, 0, 1) == 1)
        {
            VocabularyCache::Instance().Save();
        }
    }
}
