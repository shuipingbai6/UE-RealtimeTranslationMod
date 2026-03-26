#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <functional>
#include <chrono>
#include <thread>
#include <condition_variable>

#include <Mod/CppUserModBase.hpp>
#include <Unreal/FText.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UFunction.hpp>

#define NOMINMAX
#include <windows.h>

namespace RC::RealtimeTranslation
{
    // 前向声明
    class ConfigManager;
    class VocabularyCache;
    class TranslationManager;
    class TextHookManager;
    class TextApplicator;
    class AIProvider;
    class NetworkClient;
    namespace UI { class TranslationUI; }

    // 翻译请求结构
    struct TranslationRequest
    {
        std::wstring OriginalText;
        std::wstring Context;           // 上下文信息（可选）
        void* WidgetPtr;                // 关联的Widget指针（用于结果应用）
        std::chrono::steady_clock::time_point Timestamp;
        int RetryCount{0};
    };

    // 翻译结果结构
    struct TranslationResult
    {
        std::wstring OriginalText;
        std::wstring TranslatedText;
        bool Success{false};
        std::wstring ErrorMessage;
        std::chrono::milliseconds Duration{0};
    };

    // 配置结构
    struct AIProviderConfig
    {
        std::wstring ApiEndpoint;
        std::wstring ApiKey;
        std::wstring Model;
        int TimeoutMs{10000};
        int MaxRetries{3};
        int RequestIntervalMs{500};
        int MaxRequestsPerSecond{2};
    };

    struct TranslationConfig
    {
        std::wstring SourceLanguage{L"auto"};
        std::wstring TargetLanguage{L"zh-CN"};
    };

    struct HookFilterConfig
    {
        size_t MinTextLength{2};
        std::vector<std::wstring> ExcludePatterns;
    };

    struct VocabularyConfig
    {
        std::wstring FilePath{L"vocabulary.json"};
        bool AutoSave{true};
        int SaveIntervalMs{5000};
    };

    struct ModConfig
    {
        AIProviderConfig AIProvider;
        TranslationConfig Translation;
        HookFilterConfig HookFilter;
        VocabularyConfig Vocabulary;
        bool EnableTranslation{false};
    };

    // 统计信息
    struct TranslationStats
    {
        std::atomic<size_t> TotalTextsProcessed{0};
        std::atomic<size_t> CacheHits{0};
        std::atomic<size_t> CacheMisses{0};
        std::atomic<size_t> TranslationSuccess{0};
        std::atomic<size_t> TranslationFailed{0};
        std::atomic<size_t> QueueSize{0};

        auto GetCacheHitRate() const -> float
        {
            size_t total = CacheHits + CacheMisses;
            return total > 0 ? static_cast<float>(CacheHits) / static_cast<float>(total) : 0.0f;
        }

        auto Reset() -> void
        {
            TotalTextsProcessed = 0;
            CacheHits = 0;
            CacheMisses = 0;
            TranslationSuccess = 0;
            TranslationFailed = 0;
            QueueSize = 0;
        }
    };

    struct WStringHash
    {
        std::size_t operator()(const std::wstring& s) const noexcept
        {
            std::size_t h = 0;
            for (auto c : s)
            {
                h ^= std::size_t(c) + 0x9e3779b9 + (h << 6) + (h >> 2);
            }
            return h;
        }
    };

    struct SharedUIMemory
    {
        CRITICAL_SECTION cs;

        volatile LONG DataValid;
        volatile LONG ConfigUpdated;
        volatile LONG VocabUpdated;
        volatile LONG RequestReloadConfig;
        volatile LONG RequestSaveVocab;
        volatile LONG RequestStartTranslation;
        volatile LONG RequestStopTranslation;

        wchar_t ApiEndpoint[512];
        wchar_t ApiKey[256];
        wchar_t Model[128];
        wchar_t TargetLang[32];

        volatile LONG TranslationActive;
        volatile LONGLONG TotalProcessed;
        volatile LONGLONG CacheHits;
        volatile LONGLONG CacheMisses;
        volatile LONGLONG QueueSize;

        wchar_t LogBuffer[8192];
        volatile LONG LogWritePos;
        volatile LONG LogReadPos;

        auto Initialize() -> void
        {
            InitializeCriticalSection(&cs);
            DataValid = 0;
            ConfigUpdated = 0;
            VocabUpdated = 0;
            RequestReloadConfig = 0;
            RequestSaveVocab = 0;
            RequestStartTranslation = 0;
            RequestStopTranslation = 0;
            ApiEndpoint[0] = L'\0';
            ApiKey[0] = L'\0';
            Model[0] = L'\0';
            TargetLang[0] = L'\0';
            TranslationActive = 0;
            TotalProcessed = 0;
            CacheHits = 0;
            CacheMisses = 0;
            QueueSize = 0;
            LogBuffer[0] = L'\0';
            LogWritePos = 0;
            LogReadPos = 0;
        }

        auto Cleanup() -> void
        {
            DeleteCriticalSection(&cs);
        }

        auto Lock() -> void { EnterCriticalSection(&cs); }
        auto Unlock() -> void { LeaveCriticalSection(&cs); }

        auto SetConfig(const AIProviderConfig& aiConfig, const TranslationConfig& transConfig) -> void
        {
            Lock();
            wcscpy_s(ApiEndpoint, aiConfig.ApiEndpoint.c_str());
            wcscpy_s(ApiKey, aiConfig.ApiKey.c_str());
            wcscpy_s(Model, aiConfig.Model.c_str());
            wcscpy_s(TargetLang, transConfig.TargetLanguage.c_str());
            DataValid = 1;
            ConfigUpdated = 1;
            Unlock();
        }

        auto GetConfig(AIProviderConfig& aiConfig, TranslationConfig& transConfig) -> void
        {
            Lock();
            aiConfig.ApiEndpoint = ApiEndpoint;
            aiConfig.ApiKey = ApiKey;
            aiConfig.Model = Model;
            transConfig.TargetLanguage = TargetLang;
            Unlock();
        }

        auto UpdateStats(bool active, size_t processed, size_t hits, size_t misses, size_t queue) -> void
        {
            Lock();
            TranslationActive = active ? 1 : 0;
            TotalProcessed = static_cast<LONGLONG>(processed);
            CacheHits = static_cast<LONGLONG>(hits);
            CacheMisses = static_cast<LONGLONG>(misses);
            QueueSize = static_cast<LONGLONG>(queue);
            Unlock();
        }

        auto GetStats(bool& active, size_t& processed, size_t& hits, size_t& misses, size_t& queue) -> void
        {
            Lock();
            active = TranslationActive != 0;
            processed = static_cast<size_t>(TotalProcessed);
            hits = static_cast<size_t>(CacheHits);
            misses = static_cast<size_t>(CacheMisses);
            queue = static_cast<size_t>(QueueSize);
            Unlock();
        }

        auto AppendLog(const wchar_t* message) -> void
        {
            Lock();
            size_t len = wcslen(message);
            size_t bufSize = 8192;
            size_t writePos = static_cast<size_t>(LogWritePos);
            
            for (size_t i = 0; i < len && writePos < bufSize - 1; i++)
            {
                LogBuffer[writePos++] = message[i];
            }
            if (writePos < bufSize)
            {
                LogBuffer[writePos++] = L'\n';
            }
            LogBuffer[writePos < bufSize ? writePos : bufSize - 1] = L'\0';
            LogWritePos = static_cast<LONG>(writePos);
            Unlock();
        }

        auto ReadLog(wchar_t* buffer, size_t bufferSize) -> void
        {
            Lock();
            wcscpy_s(buffer, bufferSize, LogBuffer);
            Unlock();
        }

        auto ClearLog() -> void
        {
            Lock();
            LogBuffer[0] = L'\0';
            LogWritePos = 0;
            LogReadPos = 0;
            Unlock();
        }
    };

    constexpr const wchar_t* SHARED_MEMORY_NAME = L"RealtimeTranslationMod_UIMemory";
    constexpr const wchar_t* WINDOW_CLASS_NAME = L"RealtimeTranslationWnd";
    constexpr size_t SHARED_MEMORY_SIZE = sizeof(SharedUIMemory);

    class SharedMemoryManager
    {
    public:
        static SharedMemoryManager& Instance()
        {
            static SharedMemoryManager instance;
            return instance;
        }

        auto Create() -> bool
        {
            if (m_hMapFile) return true;

            m_hMapFile = CreateFileMappingW(
                INVALID_HANDLE_VALUE,
                nullptr,
                PAGE_READWRITE,
                0,
                static_cast<DWORD>(SHARED_MEMORY_SIZE),
                SHARED_MEMORY_NAME
            );

            if (!m_hMapFile)
            {
                return false;
            }

            m_pSharedMem = static_cast<SharedUIMemory*>(
                MapViewOfFile(m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0)
            );

            if (!m_pSharedMem)
            {
                CloseHandle(m_hMapFile);
                m_hMapFile = nullptr;
                return false;
            }

            m_isOwner = true;
            m_pSharedMem->Initialize();
            return true;
        }

        auto Open() -> bool
        {
            if (m_hMapFile) return true;

            m_hMapFile = OpenFileMappingW(
                FILE_MAP_ALL_ACCESS,
                FALSE,
                SHARED_MEMORY_NAME
            );

            if (!m_hMapFile)
            {
                return false;
            }

            m_pSharedMem = static_cast<SharedUIMemory*>(
                MapViewOfFile(m_hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0)
            );

            if (!m_pSharedMem)
            {
                CloseHandle(m_hMapFile);
                m_hMapFile = nullptr;
                return false;
            }

            m_isOwner = false;
            return true;
        }

        auto Close() -> void
        {
            if (m_pSharedMem)
            {
                if (m_isOwner)
                {
                    m_pSharedMem->Cleanup();
                }
                UnmapViewOfFile(m_pSharedMem);
                m_pSharedMem = nullptr;
            }

            if (m_hMapFile)
            {
                CloseHandle(m_hMapFile);
                m_hMapFile = nullptr;
            }

            m_isOwner = false;
        }

        auto Get() -> SharedUIMemory*
        {
            return m_pSharedMem;
        }

        auto IsValid() const -> bool
        {
            return m_pSharedMem != nullptr;
        }

        auto IsOwner() const -> bool
        {
            return m_isOwner;
        }

        ~SharedMemoryManager()
        {
            Close();
        }

    private:
        SharedMemoryManager() = default;
        SharedMemoryManager(const SharedMemoryManager&) = delete;
        SharedMemoryManager& operator=(const SharedMemoryManager&) = delete;

        HANDLE m_hMapFile{nullptr};
        SharedUIMemory* m_pSharedMem{nullptr};
        bool m_isOwner{false};
    };

} // namespace RC::RealtimeTranslation
