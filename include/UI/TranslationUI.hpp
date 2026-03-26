#pragma once

#include "TranslationCommon.hpp"
#include <string>
#include <vector>

namespace RC::RealtimeTranslation::UI
{
    /**
     * 翻译UI组件
     * 提供ImGui配置界面
     */
    class TranslationUI
    {
    public:
        static TranslationUI& Instance();

        // 渲染UI
        auto Render() -> void;

        // 初始化
        auto Initialize() -> void;

    private:
        TranslationUI() = default;
        ~TranslationUI() = default;
        TranslationUI(const TranslationUI&) = delete;
        TranslationUI& operator=(const TranslationUI&) = delete;

        // 渲染各个UI部分
        auto RenderConfigSection() -> void;
        auto RenderStatusSection() -> void;
        auto RenderVocabularySection() -> void;
        auto RenderLogSection() -> void;

        // UI状态
        struct UIState
        {
            // 配置输入缓冲区
            char ApiEndpointBuffer[1024]{};
            char ApiKeyBuffer[512]{};
            char ModelBuffer[128]{};
            char TargetLangBuffer[32]{};

            // UI状态
            bool ShowPassword{false};
            bool ConfigChanged{false};
            int SelectedTab{0};

            // 日志
            std::vector<std::wstring> LogEntries;
            size_t MaxLogEntries{100};
        };

        UIState m_state;
        bool m_initialized{false};

        // 添加日志
        auto AddLog(const std::wstring& message) -> void;

        // 更新配置缓冲区
        auto UpdateConfigBuffers() -> void;

        // 应用配置
        auto ApplyConfig() -> bool;
    };
} // namespace RC::RealtimeTranslation::UI
