#pragma once

#include "TranslationCommon.hpp"
#include <concurrentqueue.h>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <queue>

namespace RC::RealtimeTranslation
{
    /**
     * 异步翻译管理器
     * 管理翻译任务队列和工作线程
     */
    class TranslationManager
    {
    public:
        // 翻译完成回调类型
        using TranslationCallback = std::function<void(const TranslationResult&)>;

        static TranslationManager& Instance();

        // 初始化
        auto Initialize() -> bool;

        // 关闭
        auto Shutdown() -> void;

        // 入队翻译请求
        auto Enqueue(
            const std::wstring& text,
            void* widgetPtr = nullptr,
            const std::wstring& context = L""
        ) -> bool;

        // 批量入队
        auto EnqueueBatch(const std::vector<TranslationRequest>& requests) -> void;

        // 设置翻译完成回调
        auto SetTranslationCallback(TranslationCallback callback) -> void;

        // 获取队列大小
        [[nodiscard]] auto GetQueueSize() const -> size_t;

        // 获取统计信息
        [[nodiscard]] auto GetStats() const -> const TranslationStats& { return m_stats; }

        // 重置统计
        auto ResetStats() -> void { m_stats.Reset(); }

        // 启动/停止翻译
        auto Start() -> void;
        auto Stop() -> void;
        [[nodiscard]] auto IsRunning() const -> bool { return m_running; }

        // 设置最大并发数
        auto SetMaxWorkers(int count) -> void;

        // 处理待处理的翻译结果（在游戏主线程调用）
        auto ProcessPendingResults() -> void;

    private:
        TranslationManager() = default;
        ~TranslationManager();
        TranslationManager(const TranslationManager&) = delete;
        TranslationManager& operator=(const TranslationManager&) = delete;

        // 工作线程函数
        auto WorkerThread() -> void;

        // 处理单个翻译请求
        auto ProcessRequest(TranslationRequest& request) -> TranslationResult;

        // 翻译队列
        moodycamel::ConcurrentQueue<TranslationRequest> m_requestQueue;

        // 结果队列（用于回调到主线程）
        moodycamel::ConcurrentQueue<TranslationResult> m_resultQueue;

        // 工作线程
        std::vector<std::thread> m_workers;
        std::atomic<bool> m_running{false};
        std::atomic<bool> m_stopRequested{false};
        std::condition_variable m_cv;
        std::mutex m_cvMutex;

        // 回调
        TranslationCallback m_callback;

        // 统计
        TranslationStats m_stats;

        // 配置
        int m_maxWorkers{2};
        bool m_initialized{false};
    };
} // namespace RC::RealtimeTranslation
