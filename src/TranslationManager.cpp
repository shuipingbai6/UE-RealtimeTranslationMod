#include <TranslationManager.hpp>
#include <AIProvider.hpp>
#include <VocabularyCache.hpp>
#include <EntityExtractor.hpp>
#include <ConfigManager.hpp>
#include <DynamicOutput/DynamicOutput.hpp>
#include <DebugLogger.hpp>

#include <algorithm>

namespace RC::RealtimeTranslation
{
    TranslationManager& TranslationManager::Instance()
    {
        static TranslationManager instance;
        return instance;
    }

    TranslationManager::~TranslationManager()
    {
        Shutdown();
    }

    auto TranslationManager::Initialize() -> bool
    {
        if (m_initialized) return true;

        m_stopRequested = false;
        m_initialized = true;

        Output::send<LogLevel::Normal>(STR("[TranslationManager] Initialized.\n"));
        return true;
    }

    auto TranslationManager::Shutdown() -> void
    {
        if (!m_initialized) return;

        Stop();

        m_initialized = false;
        Output::send<LogLevel::Normal>(STR("[TranslationManager] Shutdown complete.\n"));
    }

    auto TranslationManager::Enqueue(
        const std::wstring& text,
        void* widgetPtr,
        const std::wstring& context
    ) -> bool
    {
        if (!m_running)
        {
            RT_DEBUG_LOG(std::wstring(L"[TranslationManager] Enqueue rejected (not running): '") + text + L"'");
            return false;
        }

        TranslationRequest request;
        request.OriginalText = text;
        request.Context = context;
        request.WidgetPtr = widgetPtr;
        request.Timestamp = std::chrono::steady_clock::now();
        request.RetryCount = 0;

        m_requestQueue.enqueue(request);
        m_stats.QueueSize = m_requestQueue.size_approx();

        RT_DEBUG_LOG(std::wstring(L"[TranslationManager] Enqueued: '") + text + L"', queue size: " + std::to_wstring(m_stats.QueueSize.load()));
        return true;
    }

    auto TranslationManager::EnqueueBatch(const std::vector<TranslationRequest>& requests) -> void
    {
        if (!m_running) return;

        for (const auto& request : requests)
        {
            m_requestQueue.enqueue(request);
        }
        m_stats.QueueSize = m_requestQueue.size_approx();
    }

    auto TranslationManager::SetTranslationCallback(TranslationCallback callback) -> void
    {
        m_callback = std::move(callback);
    }

    auto TranslationManager::GetQueueSize() const -> size_t
    {
        return m_requestQueue.size_approx();
    }

    auto TranslationManager::Start() -> void
    {
        if (m_running)
        {
            RT_DEBUG_LOG(L"[TranslationManager] Start called but already running.");
            return;
        }

        m_stopRequested = false;
        m_running = true;

        int workerCount = std::max(1, m_maxWorkers);
        for (int i = 0; i < workerCount; ++i)
        {
            m_workers.emplace_back(&TranslationManager::WorkerThread, this);
        }

        RT_DEBUG_LOG(std::wstring(L"[TranslationManager] Started ") + std::to_wstring(workerCount) + L" worker threads.");
        Output::send<LogLevel::Normal>(STR("[TranslationManager] Started {} worker threads.\n"), workerCount);
    }

    auto TranslationManager::Stop() -> void
    {
        if (!m_running) return;

        m_stopRequested = true;
        m_running = false;
        m_cv.notify_all();

        for (auto& worker : m_workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
        m_workers.clear();

        Output::send<LogLevel::Normal>(STR("[TranslationManager] Stopped.\n"));
    }

    auto TranslationManager::SetMaxWorkers(int count) -> void
    {
        m_maxWorkers = std::max(1, count);
    }

    auto TranslationManager::ProcessPendingResults() -> void
    {
        // 处理待处理的翻译结果（在游戏主线程调用）
        TranslationResult result;
        while (m_resultQueue.try_dequeue(result))
        {
            if (m_callback)
            {
                m_callback(result);
            }
        }
    }

    auto TranslationManager::WorkerThread() -> void
    {
        while (!m_stopRequested)
        {
            TranslationRequest request;

            // 尝试从队列获取请求
            if (!m_requestQueue.try_dequeue(request))
            {
                // 队列为空，等待
                std::unique_lock<std::mutex> lock(m_cvMutex);
                m_cv.wait_for(lock, std::chrono::milliseconds(100), [this]() {
                    return m_stopRequested || m_requestQueue.size_approx() > 0;
                });
                continue;
            }

            m_stats.QueueSize = m_requestQueue.size_approx();

            // 处理请求
            auto result = ProcessRequest(request);

            // 将结果放入结果队列
            m_resultQueue.enqueue(result);

            if (result.Success)
            {
                m_stats.TranslationSuccess++;
            }
            else
            {
                m_stats.TranslationFailed++;
            }
        }
    }

    auto TranslationManager::ProcessRequest(TranslationRequest& request) -> TranslationResult
    {
        TranslationResult result;
        result.OriginalText = request.OriginalText;

        // 缓存优先级 1: 完整文本精确匹配
        auto cached = VocabularyCache::Instance().Get(request.OriginalText);
        if (cached.has_value())
        {
            result.TranslatedText = cached.value();
            result.Success = true;
            m_stats.CacheHits++;
            RT_DEBUG_LOG(std::wstring(L"[TranslationManager] Cache hit (exact): '") + request.OriginalText + L"' -> '" + result.TranslatedText + L"'");
            return result;
        }

        m_stats.CacheMisses++;
        m_stats.TotalTextsProcessed++;

        // 预处理: 提取实体
        auto extraction = EntityExtractor::ExtractEntities(request.OriginalText);
        
        // 缓存优先级 2: 模板骨架匹配
        if (!extraction.Entities.empty())
        {
            RT_DEBUG_LOG(std::wstring(L"[TranslationManager] Extracted skeleton: '") + extraction.SkeletonText + L"' with " + std::to_wstring(extraction.Entities.size()) + L" entities");
            
            auto templateEntry = VocabularyCache::Instance().GetTemplate(extraction.SkeletonText);
            if (templateEntry.has_value())
            {
                // 找到模板，填充槽位生成译文
                result.TranslatedText = EntityExtractor::FillSlots(templateEntry->TargetTemplate, extraction.Entities);
                result.Success = true;
                m_stats.CacheHits++; // 模板命中也算缓存命中
                RT_DEBUG_LOG(std::wstring(L"[TranslationManager] Template hit: '") + extraction.SkeletonText + L"' -> '" + result.TranslatedText + L"'");
                
                // 同时存储完整文本的翻译结果到词库缓存
                VocabularyCache::Instance().Store(request.OriginalText, result.TranslatedText);
                return result;
            }
        }

        // 缓存优先级 3: AI翻译
        // 检查AI提供商是否配置
        if (!AIProvider::Instance().IsConfigured())
        {
            result.ErrorMessage = L"AI provider not configured";
            return result;
        }

        // 获取翻译配置
        const auto& config = ConfigManager::Instance().GetConfig();

        // 执行翻译
        for (int retry = 0; retry <= config.AIProvider.MaxRetries; ++retry)
        {
            auto translationResult = AIProvider::Instance().Translate(
                request.OriginalText,
                config.Translation.SourceLanguage,
                config.Translation.TargetLanguage
            );

            if (translationResult.Success)
            {
                result.TranslatedText = translationResult.TranslatedText;
                result.Success = true;
                result.Duration = translationResult.Duration;

                // 存储翻译结果到词库缓存
                VocabularyCache::Instance().Store(request.OriginalText, result.TranslatedText);

                // 如果有实体，存储模板
                if (!extraction.Entities.empty())
                {
                    VocabularyCache::Instance().StoreTemplateFromTranslation(extraction, result.TranslatedText);
                    RT_DEBUG_LOG(std::wstring(L"[TranslationManager] Stored template: '") + extraction.SkeletonText + L"' -> '" + result.TranslatedText + L"'");
                }

                return result;
            }

            result.ErrorMessage = translationResult.ErrorMessage;

            // 如果不是最后一次重试，等待一段时间
            if (retry < config.AIProvider.MaxRetries)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500 * (retry + 1)));
            }
        }

        return result;
    }
} // namespace RC::RealtimeTranslation
