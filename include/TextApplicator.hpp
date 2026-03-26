#pragma once

#include "TranslationCommon.hpp"
#include <Unreal/UObject.hpp>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>

namespace RC::RealtimeTranslation
{
    /**
     * 翻译结果应用器
     * 负责将翻译结果应用到游戏UI
     */
    class TextApplicator
    {
    public:
        // 待应用的翻译项
        struct PendingApplication
        {
            std::wstring OriginalText;
            std::wstring TranslatedText;
            void* WidgetPtr;
            std::chrono::steady_clock::time_point Timestamp;
        };

        static TextApplicator& Instance();

        // 初始化
        auto Initialize() -> bool;

        // 关闭
        auto Shutdown() -> void;

        // 同步应用翻译（在Hook回调中调用，词库命中时）
        auto ApplySync(
            const std::wstring& original,
            const std::wstring& translated,
            Unreal::UObject* widget
        ) -> bool;

        // 异步应用翻译（翻译完成后调用）
        auto ApplyAsync(
            const std::wstring& original,
            const std::wstring& translated,
            void* widgetPtr
        ) -> void;

        // 处理待应用的翻译（在游戏主线程调用）
        auto ProcessPendingApplications() -> void;

        // 刷新所有匹配的Widget
        auto RefreshWidgets(const std::wstring& originalText) -> void;

        // 验证翻译结果
        [[nodiscard]] auto ValidateTranslation(
            const std::wstring& original,
            const std::wstring& translated
        ) const -> bool;

        // 设置批量应用延迟
        auto SetBatchDelay(int delayMs) -> void;

        // 获取待应用数量
        [[nodiscard]] auto GetPendingCount() const -> size_t;

    private:
        TextApplicator() = default;
        ~TextApplicator() = default;
        TextApplicator(const TextApplicator&) = delete;
        TextApplicator& operator=(const TextApplicator&) = delete;

        // 应用翻译到Widget
        auto ApplyToWidget(
            Unreal::UObject* widget,
            const std::wstring& text
        ) -> bool;

        // 查找匹配的Widget
        auto FindWidgetsByText(const std::wstring& text) -> std::vector<Unreal::UObject*>;

        // 待应用队列
        std::queue<PendingApplication> m_pendingQueue;
        mutable std::mutex m_queueMutex;

        // 配置
        int m_batchDelayMs{16}; // 默认约1帧延迟
        bool m_initialized{false};

        // Widget缓存（用于快速查找）
        std::unordered_map<std::wstring, std::vector<Unreal::UObject*>, WStringHash> m_widgetCache;
        std::shared_mutex m_cacheMutex;
    };
} // namespace RC::RealtimeTranslation
