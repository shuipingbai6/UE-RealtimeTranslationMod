#pragma once

#include "TranslationCommon.hpp"
#include <string>
#include <functional>

namespace RC::RealtimeTranslation::UI
{
    class Win32TranslationUI
    {
    public:
        static Win32TranslationUI& Instance();

        auto Initialize() -> bool;
        auto Shutdown() -> void;
        auto IsRunning() const -> bool { return m_running; }
        auto GetHwnd() const -> HWND { return m_hwnd; }

        auto SetOnConfigChanged(std::function<void(const std::wstring&, const std::wstring&, const std::wstring&, const std::wstring&)> callback) -> void
        {
            m_onConfigChanged = std::move(callback);
        }

        auto SetOnStartStop(std::function<void(bool)> callback) -> void
        {
            m_onStartStop = std::move(callback);
        }

        auto SetOnSaveVocab(std::function<void()> callback) -> void
        {
            m_onSaveVocab = std::move(callback);
        }

        auto SetOnReloadConfig(std::function<void()> callback) -> void
        {
            m_onReloadConfig = std::move(callback);
        }

        auto SetOnLoadVocab(std::function<void()> callback) -> void
        {
            m_onLoadVocab = std::move(callback);
        }

        auto SetOnRefreshWidgets(std::function<void()> callback) -> void
        {
            m_onRefreshWidgets = std::move(callback);
        }

        auto UpdateStats(bool active, size_t processed, size_t hits, size_t misses, size_t queue) -> void;

    private:
        Win32TranslationUI() = default;
        ~Win32TranslationUI() = default;
        Win32TranslationUI(const Win32TranslationUI&) = delete;
        Win32TranslationUI& operator=(const Win32TranslationUI&) = delete;

        static auto WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT;
        static auto WINAPI ThreadProc(LPVOID param) -> DWORD;

        auto RegisterWindowClass() -> bool;
        auto CreateMainWindow() -> bool;
        auto CreateControls() -> void;
        auto LayoutControls() -> void;
        auto UpdateControlsFromSharedMemory() -> void;
        auto ApplyConfigToSharedMemory() -> void;
        auto ToggleExpand() -> void;

        auto OnCommand(WPARAM wParam, LPARAM lParam) -> void;
        auto OnCreate() -> void;
        auto OnDestroy() -> void;
        auto OnNcLButtonDblClk(WPARAM wParam, LPARAM lParam) -> void;

        HWND m_hwnd{nullptr};
        HWND m_hwndLblApiEndpoint{nullptr};
        HWND m_hwndApiEndpoint{nullptr};
        HWND m_hwndLblApiKey{nullptr};
        HWND m_hwndApiKey{nullptr};
        HWND m_hwndLblModel{nullptr};
        HWND m_hwndModel{nullptr};
        HWND m_hwndLblTargetLang{nullptr};
        HWND m_hwndTargetLang{nullptr};
        HWND m_hwndStatus{nullptr};
        HWND m_hwndBtnStartStop{nullptr};
        HWND m_hwndBtnShowKey{nullptr};
        HFONT m_hFont{nullptr};

        bool m_running{false};
        bool m_expanded{false};
        bool m_showPassword{false};
        HANDLE m_threadHandle{nullptr};
        DWORD m_threadId{0};

        std::function<void(const std::wstring&, const std::wstring&, const std::wstring&, const std::wstring&)> m_onConfigChanged;
        std::function<void(bool)> m_onStartStop;
        std::function<void()> m_onSaveVocab;
        std::function<void()> m_onLoadVocab;
        std::function<void()> m_onReloadConfig;
        std::function<void()> m_onRefreshWidgets;

        static constexpr int WINDOW_WIDTH_COLLAPSED = 200;
        static constexpr int WINDOW_HEIGHT_COLLAPSED = 35;
        static constexpr int WINDOW_WIDTH_EXPANDED = 351;
        static constexpr int WINDOW_HEIGHT_EXPANDED = 380;

        static constexpr int ID_BTN_OPENAI = 1001;
        static constexpr int ID_BTN_MODELSCOPE = 1002;
        static constexpr int ID_BTN_DEEPSEEK = 1003;
        static constexpr int ID_BTN_SHOWKEY = 1004;
        static constexpr int ID_BTN_ZHCN = 1005;
        static constexpr int ID_BTN_JA = 1006;
        static constexpr int ID_BTN_KO = 1007;
        static constexpr int ID_BTN_STARTSTOP = 1008;
        static constexpr int ID_BTN_SAVECONFIG = 1009;
        static constexpr int ID_BTN_SAVEVOCAB = 1010;
        static constexpr int ID_BTN_RELOAD = 1011;
        static constexpr int ID_BTN_LOADVOCAB = 1012;
        static constexpr int ID_BTN_REFRESHWIDGETS = 1013;
        static constexpr int ID_EDIT_APIENDPOINT = 1014;
        static constexpr int ID_EDIT_APIKEY = 1015;
        static constexpr int ID_EDIT_MODEL = 1016;
        static constexpr int ID_EDIT_TARGETLANG = 1017;
    };
}
