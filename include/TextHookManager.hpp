#pragma once

#include "TranslationCommon.hpp"
#include "HookTargetSelector.hpp"
#include <Unreal/UObject.hpp>
#include <Unreal/UFunction.hpp>
#include <functional>
#include <vector>
#include <string>
#include <chrono>

namespace RC::Unreal
{
    class UTextBlock;
    class URichTextBlock;
    class UEditableText;
}

namespace RC::RealtimeTranslation
{

    /**
     * 文本Hook管理器
     * 管理多个Hook注册和回调处理
     */
    class TextHookManager
    {
    public:
        // Hook回调类型
        using TextHookCallback = std::function<void(
            Unreal::UObject* context,
            const std::wstring& originalText,
            bool& shouldBlockOriginal
        )>;

        static TextHookManager& Instance();

        // 初始化
        auto Initialize() -> bool;

        // 关闭
        auto Shutdown() -> void;

        // 注册Hook回调
        auto RegisterHookCallback(TextHookCallback callback) -> void;

        // 设置文本过滤配置
        auto SetFilterConfig(const HookFilterConfig& config) -> void;

        // 检查文本是否应该翻译
        [[nodiscard]] auto ShouldTranslate(const std::wstring& text) const -> bool;

        // 获取Hook统计
        [[nodiscard]] auto GetHookStats() const -> const std::vector<HookTargetInfo>&;

        // 获取性能统计
        [[nodiscard]] auto GetAverageHookTime() const -> std::chrono::microseconds;

        // 启用/禁用Hook
        auto EnableHooks() -> void;
        auto DisableHooks() -> void;
        [[nodiscard]] auto AreHooksEnabled() const -> bool { return m_hooksEnabled; }

    private:
        TextHookManager() = default;
        ~TextHookManager() = default;
        TextHookManager(const TextHookManager&) = delete;
        TextHookManager& operator=(const TextHookManager&) = delete;

        // Hook处理函数
        auto OnProcessEvent(
            Unreal::UObject* context,
            Unreal::UFunction* function,
            void* parms
        ) -> void;

        // 处理SetText调用
        auto HandleSetTextCall(
            Unreal::UObject* context,
            Unreal::UFunction* function,
            void* parms
        ) -> void;

        // 从参数中提取文本
        auto ExtractTextFromParms(
            Unreal::UFunction* function,
            void* parms
        ) -> std::optional<std::wstring>;

        // 修改参数中的文本
        auto ModifyTextInParms(
            Unreal::UFunction* function,
            void* parms,
            const std::wstring& newText
        ) -> bool;

        std::vector<TextHookCallback> m_callbacks;
        HookFilterConfig m_filterConfig;
        std::atomic<bool> m_hooksEnabled{false};
        std::atomic<bool> m_initialized{false};

        // 性能监控
        std::atomic<uint64_t> m_totalHookTime{0};
        std::atomic<size_t> m_hookCallCount{0};

        // Hook ID
        int64_t m_processEventHookId{-1};
    };
} // namespace RC::RealtimeTranslation
