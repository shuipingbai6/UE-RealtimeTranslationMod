#include <TranslationManager.hpp>
#include <AIProvider.hpp>
#include <VocabularyCache.hpp>
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
        // Process pending translation results (called on game main thread)
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

            // Try to dequeue request
            if (!m_requestQueue.try_dequeue(request))
            {
                // Queue is empty, wait
                std::unique_lock<std::mutex> lock(m_cvMutex);
                m_cv.wait_for(lock, std::chrono::milliseconds(100), [this]() {
                    return m_stopRequested || m_requestQueue.size_approx() > 0;
                });
                continue;
            }

            m_stats.QueueSize = m_requestQueue.size_approx();

            // Process request
            auto result = ProcessRequest(request);

            // Enqueue result
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

        // Check vocabulary again (may have been translated by other thread while waiting)
        auto cached = VocabularyCache::Instance().Get(request.OriginalText);
        if (cached.has_value())
        {
            result.TranslatedText = cached.value();
            result.Success = true;
            m_stats.CacheHits++;
            return result;
        }

        m_stats.CacheMisses++;
        m_stats.TotalTextsProcessed++;

        // Check if AI provider is configured
        if (!AIProvider::Instance().IsConfigured())
        {
            result.ErrorMessage = L"AI provider not configured";
            return result;
        }

        // Get translation config
        const auto& config = ConfigManager::Instance().GetConfig();

        // Execute translation
        auto maxRetries = config.AIProvider.MaxRetries;
        for (int retry = 0; retry <= maxRetries; ++retry)
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
                return result;
            }

            result.ErrorMessage = translationResult.ErrorMessage;

            // If not last retry, wait for a while
            if (retry < maxRetries)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500 * (retry + 1)));
            }
        }

        return result;
    }
} // namespace RC::RealtimeTranslation
