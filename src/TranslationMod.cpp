#include <TranslationMod.hpp>
#include <UIPropertyScanner.hpp>
#include <Unreal/Hooks/Hooks.hpp>
#include <UE4SSProgram.hpp>
#include <DynamicOutput/DynamicOutput.hpp>
#include <DebugLogger.hpp>

#include <filesystem>

namespace RC::RealtimeTranslation
{
    TranslationMod* TranslationMod::s_instance = nullptr;

    TranslationMod::TranslationMod()
        : CppUserModBase()
    {
        s_instance = this;

        ModName = STR("RealtimeTranslationMod");
        ModAuthors = STR("jx666");
        ModDescription = STR("Real-time text translation for UE4/UE5 games using AI translation APIs.");
        ModVersion = STR("1.0.0");
        ModIntendedSDKVersion = STR("2.6");

        register_tab(STR("RealtimeTranslationMod by jx666"), [](CppUserModBase* mod) {
            UE4SS_ENABLE_IMGUI();
            UI::TranslationUI::Instance().Render();
        });

        RT_DEBUG_LOG(L"[RealtimeTranslationMod] Mod constructed.");
        Output::send<LogLevel::Normal>(STR("[RealtimeTranslationMod] Mod constructed.\n"));
    }

    TranslationMod::~TranslationMod()
    {
        ShutdownSubsystems();
        s_instance = nullptr;
        RT_DEBUG_LOG(L"[RealtimeTranslationMod] Mod destroyed.");
        Output::send<LogLevel::Normal>(STR("[RealtimeTranslationMod] Mod destroyed.\n"));
    }

    auto TranslationMod::on_unreal_init() -> void
    {
        RT_DEBUG_LOG(L"[RealtimeTranslationMod] on_unreal_init called.");
        Output::send<LogLevel::Normal>(STR("[RealtimeTranslationMod] on_unreal_init called.\n"));

        m_unrealLoaded.test_and_set();

        if (!InitializeSubsystems())
        {
            RT_DEBUG_LOG(L"[RealtimeTranslationMod] Failed to initialize subsystems.");
            Output::send<LogLevel::Error>(STR("[RealtimeTranslationMod] Failed to initialize subsystems.\n"));
            return;
        }

        RT_DEBUG_LOG(L"[RealtimeTranslationMod] Registering EngineTickPostCallback...");
        Unreal::Hook::RegisterEngineTickPostCallback(
            [](Unreal::Hook::TCallbackIterationData<void>& callback_data,
               Unreal::UEngine*,
               float,
               bool) {
                TextApplicator::Instance().ProcessPendingApplications();
                TranslationManager::Instance().ProcessPendingResults();
                TranslationMod::GetInstance()->ProcessMainThreadQueue();
                UIPropertyScanner::Instance().ScanAllWidgets();
            },
            {false, false, STR("RealtimeTranslationMod"), STR("EngineTickPost")}
        );

        RT_DEBUG_LOG(L"[RealtimeTranslationMod] Unreal initialization complete.");
        Output::send<LogLevel::Normal>(STR("[RealtimeTranslationMod] Unreal initialization complete.\n"));

        RT_DEBUG_LOG(L"[RealtimeTranslationMod] Initializing Win32 UI...");
        InitializeWin32UI();
    }

    auto TranslationMod::InitializeWin32UI() -> void
    {
        RT_DEBUG_LOG(L"[RealtimeTranslationMod] InitializeWin32UI called.");
        
        auto& win32UI = UI::Win32TranslationUI::Instance();
        win32UI.SetOnConfigChanged([this](const std::wstring& endpoint, const std::wstring& key,
            const std::wstring& model, const std::wstring& targetLang) {
            auto config = ConfigManager::Instance().GetConfigMutable();
            config.AIProvider.ApiEndpoint = endpoint;
            config.AIProvider.ApiKey = key;
            config.AIProvider.Model = model;
            config.Translation.TargetLanguage = targetLang;
            ConfigManager::Instance().UpdateConfig(config);
            AIProvider::Instance().Initialize(config.AIProvider);
        });

        win32UI.SetOnStartStop([this](bool start) {
            if (start)
            {
                StartTranslation();
            }
            else
            {
                StopTranslation();
            }
        });

        win32UI.SetOnSaveVocab([this]() {
            VocabularyCache::Instance().Save();
        });

        win32UI.SetOnReloadConfig([this]() {
            ConfigManager::Instance().LoadConfig();
            AIProvider::Instance().Initialize(ConfigManager::Instance().GetConfig().AIProvider);
        });

        if (win32UI.Initialize())
        {
            RT_DEBUG_LOG(L"[RealtimeTranslationMod] Win32 UI initialized successfully.");
            Output::send<LogLevel::Normal>(STR("[RealtimeTranslationMod] Win32 UI initialized.\n"));
        }
        else
        {
            RT_DEBUG_LOG(L"[RealtimeTranslationMod] Win32 UI initialization failed.");
        }
    }

    auto TranslationMod::on_ui_init() -> void
    {
        Output::send<LogLevel::Normal>(STR("[RealtimeTranslationMod] on_ui_init called.\n"));
        UI::TranslationUI::Instance().Initialize();
    }

    auto TranslationMod::on_update() -> void
    {
        if (!m_initialized) return;

        ConfigManager::Instance().CheckSharedMemoryRequests();

        auto* shm = SharedMemoryManager::Instance().Get();
        if (shm)
        {
            if (InterlockedCompareExchange(&shm->RequestStartTranslation, 0, 1) == 1)
            {
                StartTranslation();
            }
            if (InterlockedCompareExchange(&shm->RequestStopTranslation, 0, 1) == 1)
            {
                StopTranslation();
            }

            const auto& stats = GetStats();
            shm->UpdateStats(
                m_translationActive.load(),
                stats.TotalTextsProcessed.load(),
                stats.CacheHits.load(),
                stats.CacheMisses.load(),
                stats.QueueSize.load()
            );
        }
    }

    auto TranslationMod::InitializeSubsystems() -> bool
    {
        if (m_initialized) return true;

        try
        {
            RT_DEBUG_LOG(L"[RealtimeTranslationMod] InitializeSubsystems: Starting...");

            auto modDir = std::filesystem::current_path() / "Mods" / "RealtimeTranslationMod";
            m_configPath = modDir.wstring() + L"\\translation_config.json";
            m_vocabularyPath = modDir.wstring() + L"\\vocabulary.json";

            DebugLogger::Instance().Initialize(modDir.wstring() + L"\\debug.log");

            RT_DEBUG_LOG(L"[RealtimeTranslationMod] InitializeSubsystems: Creating directories...");
            std::filesystem::create_directories(modDir);

            RT_DEBUG_LOG(L"[RealtimeTranslationMod] InitializeSubsystems: Initializing ConfigManager...");
            if (!ConfigManager::Instance().Initialize(m_configPath))
            {
                RT_DEBUG_LOG(L"[RealtimeTranslationMod] Failed to initialize config manager, using defaults.");
            }

            RT_DEBUG_LOG(L"[RealtimeTranslationMod] InitializeSubsystems: Initializing NetworkClient...");
            if (!NetworkClient::Instance().Initialize())
            {
                RT_DEBUG_LOG(L"[RealtimeTranslationMod] Failed to initialize network client.");
            }

            RT_DEBUG_LOG(L"[RealtimeTranslationMod] InitializeSubsystems: Initializing VocabularyCache...");
            if (!VocabularyCache::Instance().Initialize(m_vocabularyPath))
            {
                RT_DEBUG_LOG(L"[RealtimeTranslationMod] Failed to initialize vocabulary cache.");
            }

            RT_DEBUG_LOG(L"[RealtimeTranslationMod] InitializeSubsystems: Initializing AIProvider...");
            AIProvider::Instance().Initialize(ConfigManager::Instance().GetConfig().AIProvider);

            RT_DEBUG_LOG(L"[RealtimeTranslationMod] InitializeSubsystems: Creating shared memory...");
            if (SharedMemoryManager::Instance().Create())
            {
                ConfigManager::Instance().SyncToSharedMemory();
                RT_DEBUG_LOG(L"[RealtimeTranslationMod] Shared memory created and config synced.");
            }
            else
            {
                RT_DEBUG_LOG(L"[RealtimeTranslationMod] Failed to create shared memory, continuing without UI sync.");
            }

            RT_DEBUG_LOG(L"[RealtimeTranslationMod] InitializeSubsystems: Initializing TranslationManager...");
            TranslationManager::Instance().Initialize();
            TranslationManager::Instance().SetTranslationCallback(
                [this](const TranslationResult& result) {
                    OnTranslationComplete(result);
                }
            );

            RT_DEBUG_LOG(L"[RealtimeTranslationMod] InitializeSubsystems: Initializing TextHookManager...");
            TextHookManager::Instance().Initialize();
            TextHookManager::Instance().SetFilterConfig(ConfigManager::Instance().GetConfig().HookFilter);
            TextHookManager::Instance().RegisterHookCallback(
                [this](Unreal::UObject* context, const std::wstring& originalText, bool& shouldBlockOriginal) {
                    auto translated = VocabularyCache::Instance().Get(originalText);
                    if (translated.has_value())
                    {
                        TextApplicator::Instance().ApplySync(originalText, translated.value(), context);
                        shouldBlockOriginal = false;
                    }
                    else
                    {
                        TranslationManager::Instance().Enqueue(originalText, context);
                        shouldBlockOriginal = false;
                    }
                }
            );

            RT_DEBUG_LOG(L"[RealtimeTranslationMod] InitializeSubsystems: Initializing TextApplicator...");
            TextApplicator::Instance().Initialize();

            RT_DEBUG_LOG(L"[RealtimeTranslationMod] InitializeSubsystems: Initializing UIPropertyScanner...");
            UIPropertyScanner::Instance().Initialize();
            UIPropertyScanner::Instance().SetTranslationCallback(
                [this](const std::wstring& text, Unreal::UObject* obj, Unreal::FProperty* prop, size_t offset) {
                    auto translated = VocabularyCache::Instance().Get(text);
                    if (translated.has_value())
                    {
                        RT_DEBUG_LOG(std::wstring(L"[UIPropertyScanner] Vocabulary hit: '") + text + L"' -> '" + translated.value() + L"'");
                        UIPropertyScanner::Instance().SetTextPropertyValue(prop, 
                            reinterpret_cast<uint8_t*>(obj) + offset, translated.value());
                    }
                    else
                    {
                        RT_DEBUG_LOG(std::wstring(L"[UIPropertyScanner] Vocabulary miss: '") + text + L"'");
                        TranslationManager::Instance().Enqueue(text, obj);
                    }
                }
            );

            m_initialized = true;
            RT_DEBUG_LOG(L"[RealtimeTranslationMod] All subsystems initialized.");
            return true;
        }
        catch (const std::exception& e)
        {
            std::string msg = std::string("[RealtimeTranslationMod] Exception: ") + e.what();
            RT_DEBUG_LOG(msg.c_str());
            return false;
        }
    }

    auto TranslationMod::ShutdownSubsystems() -> void
    {
        if (!m_initialized) return;

        StopTranslation();

        UI::Win32TranslationUI::Instance().Shutdown();
        TextHookManager::Instance().Shutdown();
        TranslationManager::Instance().Shutdown();
        VocabularyCache::Instance().Save();
        NetworkClient::Instance().Cleanup();
        SharedMemoryManager::Instance().Close();

        m_initialized = false;
        Output::send<LogLevel::Normal>(STR("[RealtimeTranslationMod] All subsystems shut down.\n"));
    }

    auto TranslationMod::StartTranslation() -> void
    {
        RT_DEBUG_LOG(L"[TranslationMod] StartTranslation called.");
        
        if (m_translationActive)
        {
            RT_DEBUG_LOG(L"[TranslationMod] Already active, returning.");
            return;
        }

        if (!AIProvider::Instance().IsConfigured())
        {
            RT_DEBUG_LOG(L"[TranslationMod] AI provider not configured, but starting scanner anyway.");
        }

        m_translationActive = true;
        TextHookManager::Instance().EnableHooks();
        UIPropertyScanner::Instance().StartScanning();
        TranslationManager::Instance().Start();

        RT_DEBUG_LOG(L"[TranslationMod] Translation started.");
        Output::send<LogLevel::Normal>(STR("[RealtimeTranslationMod] Translation started.\n"));
    }

    auto TranslationMod::StopTranslation() -> void
    {
        if (!m_translationActive) return;

        m_translationActive = false;
        TextHookManager::Instance().DisableHooks();
        UIPropertyScanner::Instance().StopScanning();
        TranslationManager::Instance().Stop();

        Output::send<LogLevel::Normal>(STR("[RealtimeTranslationMod] Translation stopped.\n"));
    }

    auto TranslationMod::OnTranslationComplete(const TranslationResult& result) -> void
    {
        if (result.Success)
        {
            VocabularyCache::Instance().Store(result.OriginalText, result.TranslatedText);

            RT_DEBUG_LOG(std::wstring(L"[TranslationMod] Translation complete: '") + result.OriginalText + L"' -> '" + result.TranslatedText + L"'");

            Output::send<LogLevel::Verbose>(STR("[RealtimeTranslationMod] Translation complete: {} -> {}\n"),
                result.OriginalText, result.TranslatedText);
        }
        else
        {
            Output::send<LogLevel::Warning>(STR("[RealtimeTranslationMod] Translation failed for '{}': {}\n"),
                result.OriginalText, result.ErrorMessage);
        }
    }

    auto TranslationMod::RefreshAllWidgets() -> void
    {
        UIPropertyScanner::Instance().ClearScannedObjects();
        RT_DEBUG_LOG(L"[TranslationMod] Widget cache cleared, will rescan on next tick.");
    }

    auto TranslationMod::ExecuteOnGameThread(std::function<void()> func) -> void
    {
        std::lock_guard<std::mutex> lock(m_mainThreadQueueMutex);
        m_mainThreadQueue.push(std::move(func));
    }

    auto TranslationMod::ProcessMainThreadQueue() -> void
    {
        std::queue<std::function<void()>> localQueue;
        {
            std::lock_guard<std::mutex> lock(m_mainThreadQueueMutex);
            localQueue = std::move(m_mainThreadQueue);
            m_mainThreadQueue = {};
        }

        while (!localQueue.empty())
        {
            auto& func = localQueue.front();
            if (func)
            {
                func();
            }
            localQueue.pop();
        }
    }

    auto TranslationMod::GetStats() const -> const TranslationStats&
    {
        return TranslationManager::Instance().GetStats();
    }
}
