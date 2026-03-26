#pragma once

#include "TranslationCommon.hpp"
#include <string>
#include <functional>
#include <memory>

namespace RC::RealtimeTranslation
{
    /**
     * 网络客户端
     * 封装WinHTTP API，支持HTTPS请求
     */
    class NetworkClient
    {
    public:
        static NetworkClient& Instance();

        // 初始化
        auto Initialize() -> bool;

        // 清理资源
        auto Cleanup() -> void;

        // 发送HTTP POST请求
        struct HttpRequestResult
        {
            bool Success{false};
            int StatusCode{0};
            std::wstring ResponseBody;
            std::wstring ErrorMessage;
            std::chrono::milliseconds Duration{0};
        };

        auto Post(
            const std::wstring& url,
            const std::wstring& requestBody,
            const std::wstring& contentType = L"application/json",
            const std::wstring& authorization = L"",
            int timeoutMs = 10000
        ) -> HttpRequestResult;

        // URL解析
        struct ParsedUrl
        {
            std::wstring Scheme;
            std::wstring Host;
            int Port{443};
            std::wstring Path;
            bool IsHttps{true};
        };

        static auto ParseUrl(const std::wstring& url) -> ParsedUrl;

        // 设置代理（可选）
        auto SetProxy(const std::wstring& proxyServer, const std::wstring& proxyBypass = L"") -> void;
        auto ClearProxy() -> void;

    private:
        NetworkClient() = default;
        ~NetworkClient();
        NetworkClient(const NetworkClient&) = delete;
        NetworkClient& operator=(const NetworkClient&) = delete;

        // WinHTTP内部实现
        auto PerformRequest(
            const ParsedUrl& parsedUrl,
            const std::wstring& method,
            const std::wstring& requestBody,
            const std::wstring& contentType,
            const std::wstring& authorization,
            int timeoutMs
        ) -> HttpRequestResult;

        void* m_hSession{nullptr};  // HINTERNET
        std::wstring m_proxyServer;
        std::wstring m_proxyBypass;
        bool m_initialized{false};
    };
} // namespace RC::RealtimeTranslation
