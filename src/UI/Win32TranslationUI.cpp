#include <UI/Win32TranslationUI.hpp>
#include <DebugLogger.hpp>
#include <commctrl.h>

namespace RC::RealtimeTranslation::UI
{
    static Win32TranslationUI* g_pThis = nullptr;

    Win32TranslationUI& Win32TranslationUI::Instance()
    {
        static Win32TranslationUI instance;
        return instance;
    }

    auto Win32TranslationUI::Initialize() -> bool
    {
        RT_DEBUG_LOG(L"[Win32TranslationUI] Initialize called.");
        
        if (m_running)
        {
            RT_DEBUG_LOG(L"[Win32TranslationUI] Already running, returning true.");
            return true;
        }

        g_pThis = this;

        if (!SharedMemoryManager::Instance().Open())
        {
            RT_DEBUG_LOG(L"[Win32TranslationUI] Failed to open shared memory, creating...");
            if (!SharedMemoryManager::Instance().Create())
            {
                RT_DEBUG_LOG(L"[Win32TranslationUI] Failed to create shared memory.");
                return false;
            }
            RT_DEBUG_LOG(L"[Win32TranslationUI] Shared memory created.");
        }
        else
        {
            RT_DEBUG_LOG(L"[Win32TranslationUI] Shared memory opened.");
        }

        m_threadHandle = CreateThread(
            nullptr,
            0,
            ThreadProc,
            this,
            0,
            &m_threadId
        );

        if (!m_threadHandle)
        {
            RT_DEBUG_LOG(L"[Win32TranslationUI] Failed to create thread.");
            return false;
        }

        RT_DEBUG_LOG(L"[Win32TranslationUI] Thread created, waiting for initialization...");
        
        int waitCount = 0;
        while (!m_hwnd && waitCount < 100)
        {
            Sleep(50);
            waitCount++;
        }

        if (!m_hwnd)
        {
            RT_DEBUG_LOG(L"[Win32TranslationUI] Window creation timeout.");
            m_running = false;
            return false;
        }

        RT_DEBUG_LOG(L"[Win32TranslationUI] Initialize completed successfully.");
        return true;
    }

    auto Win32TranslationUI::Shutdown() -> void
    {
        if (!m_running) return;

        m_running = false;

        if (m_hwnd)
        {
            PostMessage(m_hwnd, WM_CLOSE, 0, 0);
        }

        if (m_threadHandle)
        {
            WaitForSingleObject(m_threadHandle, 5000);
            CloseHandle(m_threadHandle);
            m_threadHandle = nullptr;
        }

        SharedMemoryManager::Instance().Close();
    }

    auto WINAPI Win32TranslationUI::ThreadProc(LPVOID param) -> DWORD
    {
        auto* pThis = static_cast<Win32TranslationUI*>(param);
        RT_DEBUG_LOG(L"[Win32TranslationUI] ThreadProc started.");

        pThis->m_running = true;

        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
        InitCommonControlsEx(&icc);

        RT_DEBUG_LOG(L"[Win32TranslationUI] Registering window class...");
        if (!pThis->RegisterWindowClass())
        {
            RT_DEBUG_LOG(L"[Win32TranslationUI] Failed to register window class.");
            pThis->m_running = false;
            return 1;
        }
        RT_DEBUG_LOG(L"[Win32TranslationUI] Window class registered.");

        RT_DEBUG_LOG(L"[Win32TranslationUI] Creating main window...");
        if (!pThis->CreateMainWindow())
        {
            RT_DEBUG_LOG(L"[Win32TranslationUI] Failed to create main window.");
            pThis->m_running = false;
            return 1;
        }
        RT_DEBUG_LOG(L"[Win32TranslationUI] Main window created.");

        MSG msg;
        RT_DEBUG_LOG(L"[Win32TranslationUI] Entering message loop.");
        while (pThis->m_running && GetMessage(&msg, nullptr, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        RT_DEBUG_LOG(L"[Win32TranslationUI] Message loop exited.");

        return 0;
    }

    auto Win32TranslationUI::RegisterWindowClass() -> bool
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = WINDOW_CLASS_NAME;

        ATOM result = RegisterClassExW(&wc);
        RT_DEBUG_LOG(std::wstring(L"[Win32TranslationUI] RegisterClassExW result: ") + std::to_wstring(result));
        return result != 0;
    }

    auto Win32TranslationUI::CreateMainWindow() -> bool
    {
        RT_DEBUG_LOG(L"[Win32TranslationUI] CreateMainWindow called.");
        
        m_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_CONTROLPARENT,
            WINDOW_CLASS_NAME,
            L"RealtimeTranslationMod by jx666",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
            100, 100,
            WINDOW_WIDTH_EXPANDED, WINDOW_HEIGHT_EXPANDED,
            nullptr, nullptr,
            GetModuleHandle(nullptr),
            this
        );

        if (!m_hwnd)
        {
            DWORD error = GetLastError();
            RT_DEBUG_LOG(std::wstring(L"[Win32TranslationUI] CreateWindowExW failed, error: ") + std::to_wstring(error));
            return false;
        }

        RT_DEBUG_LOG(std::wstring(L"[Win32TranslationUI] Window created, hwnd: ") + std::to_wstring(reinterpret_cast<UINT_PTR>(m_hwnd)));
        
        ShowWindow(m_hwnd, SW_SHOWNORMAL);
        UpdateWindow(m_hwnd);
        
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        RT_DEBUG_LOG(std::wstring(L"[Win32TranslationUI] Client rect: ") + std::to_wstring(rc.left) + L"," + std::to_wstring(rc.top) + L"," + std::to_wstring(rc.right) + L"," + std::to_wstring(rc.bottom));

        return true;
    }

    auto WINAPI Win32TranslationUI::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT
    {
        Win32TranslationUI* pThis = nullptr;

        if (msg == WM_CREATE)
        {
            auto* pCreateStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
            pThis = reinterpret_cast<Win32TranslationUI*>(pCreateStruct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        }
        else
        {
            pThis = reinterpret_cast<Win32TranslationUI*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        switch (msg)
        {
        case WM_CREATE:
            if (pThis)
            {
                pThis->m_hwnd = hwnd;
                pThis->OnCreate();
            }
            return 0;

        case WM_COMMAND:
            if (pThis) pThis->OnCommand(wParam, lParam);
            return 0;

        case WM_NCLBUTTONDBLCLK:
            if (pThis) pThis->OnNcLButtonDblClk(wParam, lParam);
            return 0;

        case WM_LBUTTONDOWN:
            if (pThis && pThis->m_hwnd)
            {
                ReleaseCapture();
                SendMessage(pThis->m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
            return 0;

        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;

        case WM_DESTROY:
            if (pThis) pThis->OnDestroy();
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }

    auto Win32TranslationUI::OnCreate() -> void
    {
        RT_DEBUG_LOG(L"[Win32TranslationUI] OnCreate called.");
        m_expanded = true;
        CreateControls();
        LayoutControls();
        UpdateControlsFromSharedMemory();
        RT_DEBUG_LOG(L"[Win32TranslationUI] OnCreate completed.");
    }

    auto Win32TranslationUI::OnDestroy() -> void
    {
        if (m_hFont)
        {
            DeleteObject(m_hFont);
            m_hFont = nullptr;
        }
        m_hwnd = nullptr;
        m_running = false;
    }

    auto Win32TranslationUI::OnNcLButtonDblClk(WPARAM wParam, LPARAM lParam) -> void
    {
        if (wParam == HTCAPTION)
        {
            ToggleExpand();
        }
    }

    auto Win32TranslationUI::CreateControls() -> void
    {
        RT_DEBUG_LOG(L"[Win32TranslationUI] CreateControls called.");
        RT_DEBUG_LOG(std::wstring(L"[Win32TranslationUI] Parent hwnd: ") + std::to_wstring(reinterpret_cast<UINT_PTR>(m_hwnd)));
        
        m_hFont = CreateFontW(
            -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
        
        if (!m_hFont)
        {
            RT_DEBUG_LOG(L"[Win32TranslationUI] CreateFontW failed, using DEFAULT_GUI_FONT");
            m_hFont = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        }
        RT_DEBUG_LOG(std::wstring(L"[Win32TranslationUI] Font created: ") + std::to_wstring(reinterpret_cast<UINT_PTR>(m_hFont)));

        m_hwndLblApiEndpoint = CreateWindowExW(
            0, L"STATIC", L"API Endpoint:",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            5, 5, 330, 18,
            m_hwnd, nullptr,
            GetModuleHandle(nullptr), nullptr);
        RT_DEBUG_LOG(std::wstring(L"[Win32TranslationUI] Label hwnd: ") + std::to_wstring(reinterpret_cast<UINT_PTR>(m_hwndLblApiEndpoint)));
        if (m_hwndLblApiEndpoint) SendMessage(m_hwndLblApiEndpoint, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        m_hwndApiEndpoint = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            5, 25, 330, 22,
            m_hwnd, reinterpret_cast<HMENU>(ID_EDIT_APIENDPOINT),
            GetModuleHandle(nullptr), nullptr);
        RT_DEBUG_LOG(std::wstring(L"[Win32TranslationUI] Edit hwnd: ") + std::to_wstring(reinterpret_cast<UINT_PTR>(m_hwndApiEndpoint)));
        if (m_hwndApiEndpoint) SendMessage(m_hwndApiEndpoint, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        RT_DEBUG_LOG(L"[Win32TranslationUI] API Endpoint controls created.");

        m_hwndLblApiKey = CreateWindowExW(
            0, L"STATIC", L"API Key:",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            5, 55, 260, 18,
            m_hwnd, nullptr,
            GetModuleHandle(nullptr), nullptr);
        if (m_hwndLblApiKey) SendMessage(m_hwndLblApiKey, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        m_hwndApiKey = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
            5, 75, 260, 22,
            m_hwnd, reinterpret_cast<HMENU>(ID_EDIT_APIKEY),
            GetModuleHandle(nullptr), nullptr);
        if (m_hwndApiKey) SendMessage(m_hwndApiKey, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        m_hwndBtnShowKey = CreateWindowExW(
            0, L"BUTTON", L"Show",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            270, 75, 65, 22,
            m_hwnd, reinterpret_cast<HMENU>(ID_BTN_SHOWKEY),
            GetModuleHandle(nullptr), nullptr);
        if (m_hwndBtnShowKey) SendMessage(m_hwndBtnShowKey, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        RT_DEBUG_LOG(L"[Win32TranslationUI] API Key controls created.");

        m_hwndLblModel = CreateWindowExW(
            0, L"STATIC", L"Model:",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            5, 105, 330, 18,
            m_hwnd, nullptr,
            GetModuleHandle(nullptr), nullptr);
        if (m_hwndLblModel) SendMessage(m_hwndLblModel, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        m_hwndModel = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            5, 125, 330, 22,
            m_hwnd, reinterpret_cast<HMENU>(ID_EDIT_MODEL),
            GetModuleHandle(nullptr), nullptr);
        if (m_hwndModel) SendMessage(m_hwndModel, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        RT_DEBUG_LOG(L"[Win32TranslationUI] Model controls created.");

        m_hwndLblTargetLang = CreateWindowExW(
            0, L"STATIC", L"Target Language:",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            5, 155, 330, 18,
            m_hwnd, nullptr,
            GetModuleHandle(nullptr), nullptr);
        if (m_hwndLblTargetLang) SendMessage(m_hwndLblTargetLang, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        m_hwndTargetLang = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            5, 175, 330, 22,
            m_hwnd, reinterpret_cast<HMENU>(ID_EDIT_TARGETLANG),
            GetModuleHandle(nullptr), nullptr);
        if (m_hwndTargetLang) SendMessage(m_hwndTargetLang, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        RT_DEBUG_LOG(L"[Win32TranslationUI] Target Language controls created.");

        m_hwndStatus = CreateWindowExW(
            0, L"STATIC", L"Status: Inactive",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            110, 255, 225, 18,
            m_hwnd, nullptr,
            GetModuleHandle(nullptr), nullptr);
        if (m_hwndStatus) SendMessage(m_hwndStatus, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        RT_DEBUG_LOG(L"[Win32TranslationUI] Status control created.");

        m_hwndBtnStartStop = CreateWindowExW(
            0, L"BUTTON", L"Start",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 255, 100, 24,
            m_hwnd, reinterpret_cast<HMENU>(ID_BTN_STARTSTOP),
            GetModuleHandle(nullptr), nullptr);
        if (m_hwndBtnStartStop) SendMessage(m_hwndBtnStartStop, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        RT_DEBUG_LOG(L"[Win32TranslationUI] Buttons created.");

        HWND hBtn;
        hBtn = CreateWindowExW(0, L"BUTTON", L"OpenAI", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 225, 80, 24, m_hwnd, reinterpret_cast<HMENU>(ID_BTN_OPENAI), GetModuleHandle(nullptr), nullptr);
        if (hBtn) SendMessage(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        hBtn = CreateWindowExW(0, L"BUTTON", L"ModelScope", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            90, 225, 80, 24, m_hwnd, reinterpret_cast<HMENU>(ID_BTN_MODELSCOPE), GetModuleHandle(nullptr), nullptr);
        if (hBtn) SendMessage(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        hBtn = CreateWindowExW(0, L"BUTTON", L"DeepSeek", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            175, 225, 80, 24, m_hwnd, reinterpret_cast<HMENU>(ID_BTN_DEEPSEEK), GetModuleHandle(nullptr), nullptr);
        if (hBtn) SendMessage(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        hBtn = CreateWindowExW(0, L"BUTTON", L"zh-CN", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 205, 60, 24, m_hwnd, reinterpret_cast<HMENU>(ID_BTN_ZHCN), GetModuleHandle(nullptr), nullptr);
        if (hBtn) SendMessage(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        hBtn = CreateWindowExW(0, L"BUTTON", L"ja", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            70, 205, 60, 24, m_hwnd, reinterpret_cast<HMENU>(ID_BTN_JA), GetModuleHandle(nullptr), nullptr);
        if (hBtn) SendMessage(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        hBtn = CreateWindowExW(0, L"BUTTON", L"ko", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            135, 205, 60, 24, m_hwnd, reinterpret_cast<HMENU>(ID_BTN_KO), GetModuleHandle(nullptr), nullptr);
        if (hBtn) SendMessage(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        hBtn = CreateWindowExW(0, L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            5, 285, 75, 24, m_hwnd, reinterpret_cast<HMENU>(ID_BTN_SAVECONFIG), GetModuleHandle(nullptr), nullptr);
        if (hBtn) SendMessage(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        hBtn = CreateWindowExW(0, L"BUTTON", L"SaveVocab", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            85, 285, 75, 24, m_hwnd, reinterpret_cast<HMENU>(ID_BTN_SAVEVOCAB), GetModuleHandle(nullptr), nullptr);
        if (hBtn) SendMessage(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        hBtn = CreateWindowExW(0, L"BUTTON", L"LoadVocab", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            165, 285, 75, 24, m_hwnd, reinterpret_cast<HMENU>(ID_BTN_LOADVOCAB), GetModuleHandle(nullptr), nullptr);
        if (hBtn) SendMessage(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
        hBtn = CreateWindowExW(0, L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            245, 285, 75, 24, m_hwnd, reinterpret_cast<HMENU>(ID_BTN_REFRESHWIDGETS), GetModuleHandle(nullptr), nullptr);
        if (hBtn) SendMessage(hBtn, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

        RT_DEBUG_LOG(L"[Win32TranslationUI] All controls created.");
    }

    auto Win32TranslationUI::LayoutControls() -> void
    {
        if (!m_hwnd) return;

        int width = m_expanded ? WINDOW_WIDTH_EXPANDED : WINDOW_WIDTH_COLLAPSED;
        int height = m_expanded ? WINDOW_HEIGHT_EXPANDED : WINDOW_HEIGHT_COLLAPSED;

        SetWindowPos(m_hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);

        if (!m_expanded)
        {
            ShowWindow(m_hwndLblApiEndpoint, SW_HIDE);
            ShowWindow(m_hwndApiEndpoint, SW_HIDE);
            ShowWindow(m_hwndLblApiKey, SW_HIDE);
            ShowWindow(m_hwndApiKey, SW_HIDE);
            ShowWindow(m_hwndLblModel, SW_HIDE);
            ShowWindow(m_hwndModel, SW_HIDE);
            ShowWindow(m_hwndLblTargetLang, SW_HIDE);
            ShowWindow(m_hwndTargetLang, SW_HIDE);
            ShowWindow(m_hwndStatus, SW_HIDE);
            ShowWindow(m_hwndBtnStartStop, SW_HIDE);
            ShowWindow(m_hwndBtnShowKey, SW_HIDE);

            HWND hBtn;
            hBtn = GetDlgItem(m_hwnd, ID_BTN_OPENAI); if (hBtn) ShowWindow(hBtn, SW_HIDE);
            hBtn = GetDlgItem(m_hwnd, ID_BTN_MODELSCOPE); if (hBtn) ShowWindow(hBtn, SW_HIDE);
            hBtn = GetDlgItem(m_hwnd, ID_BTN_DEEPSEEK); if (hBtn) ShowWindow(hBtn, SW_HIDE);
            hBtn = GetDlgItem(m_hwnd, ID_BTN_ZHCN); if (hBtn) ShowWindow(hBtn, SW_HIDE);
            hBtn = GetDlgItem(m_hwnd, ID_BTN_JA); if (hBtn) ShowWindow(hBtn, SW_HIDE);
            hBtn = GetDlgItem(m_hwnd, ID_BTN_KO); if (hBtn) ShowWindow(hBtn, SW_HIDE);
            hBtn = GetDlgItem(m_hwnd, ID_BTN_SAVECONFIG); if (hBtn) ShowWindow(hBtn, SW_HIDE);
            hBtn = GetDlgItem(m_hwnd, ID_BTN_SAVEVOCAB); if (hBtn) ShowWindow(hBtn, SW_HIDE);
            hBtn = GetDlgItem(m_hwnd, ID_BTN_LOADVOCAB); if (hBtn) ShowWindow(hBtn, SW_HIDE);
            hBtn = GetDlgItem(m_hwnd, ID_BTN_REFRESHWIDGETS); if (hBtn) ShowWindow(hBtn, SW_HIDE);
            return;
        }

        int y = 5;
        int labelH = 18;
        int editH = 22;
        int btnH = 24;
        int gap = 4;

        SetWindowPos(m_hwndLblApiEndpoint, nullptr, 5, y, width - 10, labelH, SWP_SHOWWINDOW);
        y += labelH;
        SetWindowPos(m_hwndApiEndpoint, nullptr, 5, y, width - 10, editH, SWP_SHOWWINDOW);
        y += editH + gap;

        HWND hBtn;
        hBtn = GetDlgItem(m_hwnd, ID_BTN_OPENAI);
        if (hBtn) SetWindowPos(hBtn, nullptr, 5, y, 80, btnH, SWP_SHOWWINDOW);
        hBtn = GetDlgItem(m_hwnd, ID_BTN_MODELSCOPE);
        if (hBtn) SetWindowPos(hBtn, nullptr, 90, y, 80, btnH, SWP_SHOWWINDOW);
        hBtn = GetDlgItem(m_hwnd, ID_BTN_DEEPSEEK);
        if (hBtn) SetWindowPos(hBtn, nullptr, 175, y, 80, btnH, SWP_SHOWWINDOW);
        y += btnH + gap;

        SetWindowPos(m_hwndLblApiKey, nullptr, 5, y, width - 70, labelH, SWP_SHOWWINDOW);
        y += labelH;
        SetWindowPos(m_hwndApiKey, nullptr, 5, y, width - 70, editH, SWP_SHOWWINDOW);
        SetWindowPos(m_hwndBtnShowKey, nullptr, width - 60, y, 50, editH, SWP_SHOWWINDOW);
        y += editH + gap;

        SetWindowPos(m_hwndLblModel, nullptr, 5, y, width - 10, labelH, SWP_SHOWWINDOW);
        y += labelH;
        SetWindowPos(m_hwndModel, nullptr, 5, y, width - 10, editH, SWP_SHOWWINDOW);
        y += editH + gap;

        SetWindowPos(m_hwndLblTargetLang, nullptr, 5, y, width - 10, labelH, SWP_SHOWWINDOW);
        y += labelH;
        SetWindowPos(m_hwndTargetLang, nullptr, 5, y, width - 10, editH, SWP_SHOWWINDOW);
        y += editH + gap;

        hBtn = GetDlgItem(m_hwnd, ID_BTN_ZHCN);
        if (hBtn) SetWindowPos(hBtn, nullptr, 5, y, 60, btnH, SWP_SHOWWINDOW);
        hBtn = GetDlgItem(m_hwnd, ID_BTN_JA);
        if (hBtn) SetWindowPos(hBtn, nullptr, 70, y, 60, btnH, SWP_SHOWWINDOW);
        hBtn = GetDlgItem(m_hwnd, ID_BTN_KO);
        if (hBtn) SetWindowPos(hBtn, nullptr, 135, y, 60, btnH, SWP_SHOWWINDOW);
        y += btnH + gap;

        SetWindowPos(m_hwndBtnStartStop, nullptr, 5, y, 100, btnH, SWP_SHOWWINDOW);
        SetWindowPos(m_hwndStatus, nullptr, 110, y + 3, width - 115, labelH, SWP_SHOWWINDOW);
        y += btnH + gap;

        hBtn = GetDlgItem(m_hwnd, ID_BTN_SAVECONFIG);
        if (hBtn) SetWindowPos(hBtn, nullptr, 5, y, 75, btnH, SWP_SHOWWINDOW);
        hBtn = GetDlgItem(m_hwnd, ID_BTN_SAVEVOCAB);
        if (hBtn) SetWindowPos(hBtn, nullptr, 85, y, 75, btnH, SWP_SHOWWINDOW);
        hBtn = GetDlgItem(m_hwnd, ID_BTN_LOADVOCAB);
        if (hBtn) SetWindowPos(hBtn, nullptr, 165, y, 75, btnH, SWP_SHOWWINDOW);
        hBtn = GetDlgItem(m_hwnd, ID_BTN_REFRESHWIDGETS);
        if (hBtn) SetWindowPos(hBtn, nullptr, 245, y, 75, btnH, SWP_SHOWWINDOW);
    }

    auto Win32TranslationUI::ToggleExpand() -> void
    {
        m_expanded = !m_expanded;
        LayoutControls();
    }

    auto Win32TranslationUI::UpdateControlsFromSharedMemory() -> void
    {
        auto* shm = SharedMemoryManager::Instance().Get();
        if (!shm) return;

        AIProviderConfig aiConfig;
        TranslationConfig transConfig;
        shm->GetConfig(aiConfig, transConfig);

        SetWindowTextW(m_hwndApiEndpoint, aiConfig.ApiEndpoint.c_str());
        SetWindowTextW(m_hwndApiKey, aiConfig.ApiKey.c_str());
        SetWindowTextW(m_hwndModel, aiConfig.Model.c_str());
        SetWindowTextW(m_hwndTargetLang, transConfig.TargetLanguage.c_str());
    }

    auto Win32TranslationUI::ApplyConfigToSharedMemory() -> void
    {
        auto* shm = SharedMemoryManager::Instance().Get();
        if (!shm) return;

        wchar_t buf[512];

        GetWindowTextW(m_hwndApiEndpoint, buf, 512);
        std::wstring endpoint = buf;

        GetWindowTextW(m_hwndApiKey, buf, 256);
        std::wstring key = buf;

        GetWindowTextW(m_hwndModel, buf, 128);
        std::wstring model = buf;

        GetWindowTextW(m_hwndTargetLang, buf, 32);
        std::wstring targetLang = buf;

        AIProviderConfig aiConfig;
        aiConfig.ApiEndpoint = endpoint;
        aiConfig.ApiKey = key;
        aiConfig.Model = model;

        TranslationConfig transConfig;
        transConfig.TargetLanguage = targetLang;

        shm->SetConfig(aiConfig, transConfig);

        if (m_onConfigChanged)
        {
            m_onConfigChanged(endpoint, key, model, targetLang);
        }
    }

    auto Win32TranslationUI::OnCommand(WPARAM wParam, LPARAM lParam) -> void
    {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        switch (id)
        {
        case ID_BTN_STARTSTOP:
            {
                auto* shm = SharedMemoryManager::Instance().Get();
                if (shm)
                {
                    bool active = shm->TranslationActive != 0;
                    if (active)
                    {
                        InterlockedExchange(&shm->RequestStopTranslation, 1);
                        SetWindowTextW(m_hwndBtnStartStop, L"Start");
                        SetWindowTextW(m_hwndStatus, L"Status: Inactive");
                    }
                    else
                    {
                        InterlockedExchange(&shm->RequestStartTranslation, 1);
                        SetWindowTextW(m_hwndBtnStartStop, L"Stop");
                        SetWindowTextW(m_hwndStatus, L"Status: Active");
                    }
                    if (m_onStartStop) m_onStartStop(!active);
                }
            }
            break;

        case ID_BTN_SHOWKEY:
            m_showPassword = !m_showPassword;
            SendMessage(m_hwndApiKey, EM_SETPASSWORDCHAR, m_showPassword ? 0 : L'*', 0);
            InvalidateRect(m_hwndApiKey, nullptr, TRUE);
            SetWindowTextW(m_hwndBtnShowKey, m_showPassword ? L"Hide" : L"Show");
            break;

        case ID_BTN_OPENAI:
            SetWindowTextW(m_hwndApiEndpoint, L"https://api.openai.com/v1/chat/completions");
            SetWindowTextW(m_hwndModel, L"gpt-4");
            break;

        case ID_BTN_MODELSCOPE:
            SetWindowTextW(m_hwndApiEndpoint, L"https://api-inference.modelscope.cn/v1/chat/completions");
            SetWindowTextW(m_hwndModel, L"deepseek-ai/DeepSeek-V3.2");
            break;

        case ID_BTN_DEEPSEEK:
            SetWindowTextW(m_hwndApiEndpoint, L"https://api.deepseek.com/v1/chat/completions");
            SetWindowTextW(m_hwndModel, L"deepseek-chat");
            break;

        case ID_BTN_ZHCN:
            SetWindowTextW(m_hwndTargetLang, L"zh-CN");
            break;

        case ID_BTN_JA:
            SetWindowTextW(m_hwndTargetLang, L"ja");
            break;

        case ID_BTN_KO:
            SetWindowTextW(m_hwndTargetLang, L"ko");
            break;

        case ID_BTN_SAVECONFIG:
            ApplyConfigToSharedMemory();
            RT_DEBUG_LOG(L"[Win32TranslationUI] Configuration saved.");
            break;

        case ID_BTN_SAVEVOCAB:
            if (m_onSaveVocab) m_onSaveVocab();
            RT_DEBUG_LOG(L"[Win32TranslationUI] Vocabulary saved.");
            break;

        case ID_BTN_LOADVOCAB:
            if (m_onLoadVocab) m_onLoadVocab();
            RT_DEBUG_LOG(L"[Win32TranslationUI] Vocabulary loaded.");
            break;

        case ID_BTN_REFRESHWIDGETS:
            if (m_onRefreshWidgets) m_onRefreshWidgets();
            RT_DEBUG_LOG(L"[Win32TranslationUI] Widgets refreshed.");
            break;
        }
    }

    auto Win32TranslationUI::UpdateStats(bool active, size_t processed, size_t hits, size_t misses, size_t queue) -> void
    {
        if (!m_hwnd) return;

        auto* shm = SharedMemoryManager::Instance().Get();
        if (shm)
        {
            shm->UpdateStats(active, processed, hits, misses, queue);
        }

        wchar_t statusText[256];
        float hitRate = (hits + misses) > 0 ? (static_cast<float>(hits) / static_cast<float>(hits + misses) * 100.0f) : 0.0f;
        swprintf_s(statusText, L"Active: %s | Processed: %zu | Cache: %.1f%%",
            active ? L"Yes" : L"No", processed, hitRate);
        SetWindowTextW(m_hwndStatus, statusText);
    }
}
