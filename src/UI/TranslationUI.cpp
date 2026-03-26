#include <UI/TranslationUI.hpp>
#include <TranslationMod.hpp>
#include <ConfigManager.hpp>
#include <VocabularyCache.hpp>
#include <TranslationManager.hpp>
#include <AIProvider.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

#include <imgui.h>
#include <windows.h>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace RC::RealtimeTranslation::UI
{
    TranslationUI& TranslationUI::Instance()
    {
        static TranslationUI instance;
        return instance;
    }

    auto TranslationUI::Initialize() -> void
    {
        if (m_initialized) return;

        // Initialize config buffers
        UpdateConfigBuffers();

        m_initialized = true;
        Output::send<LogLevel::Normal>(STR("[TranslationUI] Initialized.\n"));
    }

    auto TranslationUI::Render() -> void
    {
        if (!m_initialized) return;

        // Main tab bar
        ImGui::BeginTabBar("TranslationTabs");

        if (ImGui::BeginTabItem("Config"))
        {
            RenderConfigSection();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Status"))
        {
            RenderStatusSection();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Vocabulary"))
        {
            RenderVocabularySection();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Log"))
        {
            RenderLogSection();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    auto TranslationUI::RenderConfigSection() -> void
    {
        ImGui::Spacing();

        // AI Provider configuration
        ImGui::Text("AI Provider Configuration");
        ImGui::Separator();

        ImGui::Text("API Endpoint:");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##ApiEndpoint", m_state.ApiEndpointBuffer, sizeof(m_state.ApiEndpointBuffer)))
        {
            m_state.ConfigChanged = true;
        }
        
        ImGui::Text("Presets:");
        if (ImGui::Button("OpenAI##preset"))
        {
            strcpy_s(m_state.ApiEndpointBuffer, "https://api.openai.com/v1/chat/completions");
            strcpy_s(m_state.ModelBuffer, "gpt-4");
            m_state.ConfigChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("ModelScope##preset"))
        {
            strcpy_s(m_state.ApiEndpointBuffer, "https://api-inference.modelscope.cn/v1/chat/completions");
            strcpy_s(m_state.ModelBuffer, "deepseek-ai/DeepSeek-V3.2");
            m_state.ConfigChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("DeepSeek##preset"))
        {
            strcpy_s(m_state.ApiEndpointBuffer, "https://api.deepseek.com/v1/chat/completions");
            strcpy_s(m_state.ModelBuffer, "deepseek-chat");
            m_state.ConfigChanged = true;
        }

        ImGui::Text("API Key:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##ApiKey", m_state.ApiKeyBuffer, sizeof(m_state.ApiKeyBuffer),
            m_state.ShowPassword ? 0 : ImGuiInputTextFlags_Password);
        ImGui::SameLine();
        if (ImGui::Button(m_state.ShowPassword ? "Hide" : "Show"))
        {
            m_state.ShowPassword = !m_state.ShowPassword;
        }

        ImGui::Text("Model:");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##Model", m_state.ModelBuffer, sizeof(m_state.ModelBuffer)))
        {
            m_state.ConfigChanged = true;
        }

        ImGui::Spacing();

        // Translation settings
        ImGui::Text("Translation Settings");
        ImGui::Separator();

        ImGui::Text("Target Language:");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##TargetLang", m_state.TargetLangBuffer, sizeof(m_state.TargetLangBuffer)))
        {
            m_state.ConfigChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("zh-CN##lang"))
        {
            strcpy_s(m_state.TargetLangBuffer, "zh-CN");
            m_state.ConfigChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("ja##lang"))
        {
            strcpy_s(m_state.TargetLangBuffer, "ja");
            m_state.ConfigChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("ko##lang"))
        {
            strcpy_s(m_state.TargetLangBuffer, "ko");
            m_state.ConfigChanged = true;
        }

        ImGui::Spacing();
        ImGui::Separator();

        // Buttons
        if (ImGui::Button("Apply Config", ImVec2(120, 30)))
        {
            if (ApplyConfig())
            {
                AddLog(L"Configuration applied successfully.");
            }
            else
            {
                AddLog(L"Failed to apply configuration.");
            }
        }

        ImGui::SameLine();

        // Test connection button
        if (ImGui::Button("Test Connection", ImVec2(120, 30)))
        {
            if (ApplyConfig())
            {
                auto [success, message] = AIProvider::Instance().TestConnection();
                if (success)
                {
                    AddLog(L"Connection test successful.");
                }
                else
                {
                    AddLog(L"Connection test failed: " + message);
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();

        // Translation control
        auto modInstance = TranslationMod::GetInstance();
        bool isActive = modInstance && modInstance->IsTranslationActive();

        if (!isActive)
        {
            if (ImGui::Button("Start Translation", ImVec2(140, 35)))
            {
                if (modInstance)
                {
                    modInstance->StartTranslation();
                    if (AIProvider::Instance().IsConfigured())
                    {
                        AddLog(L"Translation started with AI provider.");
                    }
                    else
                    {
                        AddLog(L"Scanner started (AI provider not configured - vocabulary matching only).");
                    }
                }
            }
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Stop Translation", ImVec2(140, 35)))
            {
                if (modInstance)
                {
                    modInstance->StopTranslation();
                    AddLog(L"Translation stopped.");
                }
            }
            ImGui::PopStyleColor();

            ImGui::SameLine();

            if (ImGui::Button("Refresh Widgets", ImVec2(140, 35)))
            {
                if (modInstance)
                {
                    modInstance->RefreshAllWidgets();
                    AddLog(L"Widget cache cleared. Will rescan on next tick.");
                }
            }
        }

        ImGui::SameLine();

        // Status indicator
        ImGui::BeginDisabled();
        ImGui::PushStyleColor(ImGuiCol_Button,
            isActive ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::Button(isActive ? "ACTIVE" : "INACTIVE", ImVec2(100, 35));
        ImGui::PopStyleColor();
        ImGui::EndDisabled();
    }

    auto TranslationUI::RenderStatusSection() -> void
    {
        ImGui::Spacing();

        auto modInstance = TranslationMod::GetInstance();
        const TranslationStats* statsPtr = modInstance ? &modInstance->GetStats() : nullptr;

        // Status cards
        ImGui::Columns(2, "status_columns", false);

        // Left column
        ImGui::SetColumnWidth(0, 250);

        ImGui::BeginChild("StatsLeft", ImVec2(0, 0), true);
        {
            ImGui::Text("Translation Statistics");
            ImGui::Separator();

            if (statsPtr)
            {
                ImGui::Text("Total Processed: %zu", statsPtr->TotalTextsProcessed.load());
                ImGui::Text("Cache Hits: %zu", statsPtr->CacheHits.load());
                ImGui::Text("Cache Misses: %zu", statsPtr->CacheMisses.load());

                float hitRate = statsPtr->GetCacheHitRate();
                ImGui::Text("Cache Hit Rate: %.1f%%", hitRate * 100.0f);
                ImGui::ProgressBar(hitRate, ImVec2(-1, 0));

                ImGui::Spacing();
                ImGui::Text("Success: %zu", statsPtr->TranslationSuccess.load());
                ImGui::Text("Failed: %zu", statsPtr->TranslationFailed.load());
                ImGui::Text("Queue Size: %zu", statsPtr->QueueSize.load());
            }
            else
            {
                ImGui::Text("Translation not active");
            }
        }
        ImGui::EndChild();

        ImGui::NextColumn();

        // Right column
        ImGui::BeginChild("StatsRight", ImVec2(0, 0), true);
        {
            ImGui::Text("Vocabulary Statistics");
            ImGui::Separator();

            auto& vocab = VocabularyCache::Instance();
            ImGui::Text("Total Entries: %zu", vocab.GetEntryCount());
            ImGui::Text("Total Queries: %zu", vocab.GetQueryCount());
            ImGui::Text("Vocabulary Hits: %zu", vocab.GetHitCount());

            float vocabHitRate = vocab.GetHitRate();
            ImGui::Text("Hit Rate: %.1f%%", vocabHitRate * 100.0f);
            ImGui::ProgressBar(vocabHitRate, ImVec2(-1, 0));

            ImGui::Spacing();

            if (ImGui::Button("Save Vocabulary"))
            {
                vocab.Save();
                AddLog(L"Vocabulary saved.");
            }

            ImGui::SameLine();

            if (ImGui::Button("Clear Stats"))
            {
                vocab.ResetStats();
                AddLog(L"Statistics cleared.");
            }
        }
        ImGui::EndChild();

        ImGui::Columns(1);
    }

    auto TranslationUI::RenderVocabularySection() -> void
    {
        ImGui::Spacing();

        auto& vocab = VocabularyCache::Instance();

        ImGui::Text("Vocabulary Management");
        ImGui::Separator();

        // Search box
        static char searchBuffer[256] = "";
        ImGui::Text("Search:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        ImGui::InputText("##Search", searchBuffer, sizeof(searchBuffer));

        ImGui::Spacing();

        // Vocabulary info
        ImGui::Text("File: %s", vocab.IsInitialized() ? "Loaded" : "Not loaded");
        ImGui::Text("Entries: %zu", vocab.GetEntryCount());

        ImGui::Spacing();

        // Action buttons
        if (ImGui::Button("Save"))
        {
            vocab.Save();
            AddLog(L"Vocabulary saved to file.");
        }

        ImGui::SameLine();

        if (ImGui::Button("Reload"))
        {
            vocab.Load();
            AddLog(L"Vocabulary reloaded from file.");
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Clear All"))
        {
            vocab.Clear();
            AddLog(L"Vocabulary cleared.");
        }
        ImGui::PopStyleColor();
    }

    auto TranslationUI::RenderLogSection() -> void
    {
        ImGui::Spacing();

        ImGui::Text("Log Output");
        ImGui::Separator();

        // 日志区域
        ImGui::BeginChild("LogArea", ImVec2(0, 0), true);

        for (const auto& entry : m_state.LogEntries)
        {
            // 转换为UTF-8
            std::string utf8Entry;
            int size = WideCharToMultiByte(CP_UTF8, 0, entry.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (size > 0)
            {
                utf8Entry.resize(size - 1);
                WideCharToMultiByte(CP_UTF8, 0, entry.c_str(), -1, &utf8Entry[0], size, nullptr, nullptr);
            }
            ImGui::TextUnformatted(utf8Entry.c_str());
        }

        // 自动滚动到底部
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();

        // 清除按钮
        if (ImGui::Button("Clear Log"))
        {
            m_state.LogEntries.clear();
        }
    }

    auto TranslationUI::AddLog(const std::wstring& message) -> void
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        std::tm tm_buf;
        localtime_s(&tm_buf, &time);

        std::wstringstream ss;
        ss << L"[" << std::put_time(&tm_buf, L"%H:%M:%S") << L"] " << message;

        m_state.LogEntries.push_back(ss.str());

        while (m_state.LogEntries.size() > m_state.MaxLogEntries)
        {
            m_state.LogEntries.erase(m_state.LogEntries.begin());
        }
    }

    auto TranslationUI::UpdateConfigBuffers() -> void
    {
        const auto& config = ConfigManager::Instance().GetConfig();

        // 宽字符转多字节（用于ImGui）
        auto wideToBuffer = [](const std::wstring& src, char* dest, size_t destSize) {
            int size = WideCharToMultiByte(CP_UTF8, 0, src.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (size > 0 && static_cast<size_t>(size) <= destSize)
            {
                WideCharToMultiByte(CP_UTF8, 0, src.c_str(), -1, dest, static_cast<int>(destSize), nullptr, nullptr);
            }
        };

        wideToBuffer(config.AIProvider.ApiEndpoint, m_state.ApiEndpointBuffer, sizeof(m_state.ApiEndpointBuffer));
        wideToBuffer(config.AIProvider.ApiKey, m_state.ApiKeyBuffer, sizeof(m_state.ApiKeyBuffer));
        wideToBuffer(config.AIProvider.Model, m_state.ModelBuffer, sizeof(m_state.ModelBuffer));
        wideToBuffer(config.Translation.TargetLanguage, m_state.TargetLangBuffer, sizeof(m_state.TargetLangBuffer));
    }

    auto TranslationUI::ApplyConfig() -> bool
    {
        // 多字节转宽字符
        auto bufferToWide = [](const char* src) -> std::wstring {
            int size = MultiByteToWideChar(CP_UTF8, 0, src, -1, nullptr, 0);
            if (size > 0)
            {
                std::wstring result(size - 1, 0);
                MultiByteToWideChar(CP_UTF8, 0, src, -1, &result[0], size);
                return result;
            }
            return L"";
        };

        ModConfig newConfig = ConfigManager::Instance().GetConfig();

        newConfig.AIProvider.ApiEndpoint = bufferToWide(m_state.ApiEndpointBuffer);
        newConfig.AIProvider.ApiKey = bufferToWide(m_state.ApiKeyBuffer);
        newConfig.AIProvider.Model = bufferToWide(m_state.ModelBuffer);
        newConfig.Translation.TargetLanguage = bufferToWide(m_state.TargetLangBuffer);

        // 更新配置
        ConfigManager::Instance().UpdateConfig(newConfig);

        // 更新AI提供商
        AIProvider::Instance().Initialize(newConfig.AIProvider);

        m_state.ConfigChanged = false;
        return true;
    }
} // namespace RC::RealtimeTranslation::UI
