#pragma once

#include "TranslationCommon.hpp"
#include "ConfigManager.hpp"
#include "VocabularyCache.hpp"
#include "TranslationManager.hpp"
#include "TextHookManager.hpp"
#include "TextApplicator.hpp"
#include "AIProvider.hpp"
#include "NetworkClient.hpp"
#include "UI/TranslationUI.hpp"
#include "UI/Win32TranslationUI.hpp"

#include <Mod/CppUserModBase.hpp>
#include <atomic>
#include <mutex>
#include <queue>
#include <functional>

namespace RC::RealtimeTranslation
{
    class TranslationMod : public CppUserModBase
    {
    public:
        TranslationMod();
        ~TranslationMod() override;

        auto on_unreal_init() -> void override;
        auto on_ui_init() -> void override;
        auto on_update() -> void override;

        static TranslationMod* GetInstance() { return s_instance; }

        auto StartTranslation() -> void;
        auto StopTranslation() -> void;
        auto RefreshAllWidgets() -> void;
        [[nodiscard]] auto IsTranslationActive() const -> bool { return m_translationActive; }

        [[nodiscard]] auto GetStats() const -> const TranslationStats&;

    private:
        auto InitializeSubsystems() -> bool;
        auto ShutdownSubsystems() -> void;
        auto OnTranslationComplete(const TranslationResult& result) -> void;
        auto ExecuteOnGameThread(std::function<void()> func) -> void;
        auto ProcessMainThreadQueue() -> void;
        auto InitializeWin32UI() -> void;

        static TranslationMod* s_instance;

        std::atomic<bool> m_translationActive{false};
        std::atomic<bool> m_initialized{false};
        std::atomic_flag m_unrealLoaded{};

        std::wstring m_configPath;
        std::wstring m_vocabularyPath;

        std::mutex m_mainThreadQueueMutex;
        std::queue<std::function<void()>> m_mainThreadQueue;
    };
}
