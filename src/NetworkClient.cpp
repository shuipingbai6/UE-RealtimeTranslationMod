#include <NetworkClient.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <sstream>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace RC::RealtimeTranslation
{
    static auto Utf8ToWide(const std::string& utf8) -> std::wstring
    {
        if (utf8.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        std::wstring result(size - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], size);
        return result;
    }

    static auto WideToUtf8(const std::wstring& wide) -> std::string
    {
        if (wide.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &result[0], size, nullptr, nullptr);
        return result;
    }

    NetworkClient& NetworkClient::Instance()
    {
        static NetworkClient instance;
        return instance;
    }

    NetworkClient::~NetworkClient()
    {
        Cleanup();
    }

    auto NetworkClient::Initialize() -> bool
    {
        if (m_initialized) return true;

        std::wstring userAgent = L"RealtimeTranslationMod/1.0";
        m_hSession = WinHttpOpen(
            userAgent.c_str(),
            m_proxyServer.empty() ? WINHTTP_ACCESS_TYPE_DEFAULT_PROXY : WINHTTP_ACCESS_TYPE_NAMED_PROXY,
            m_proxyServer.empty() ? nullptr : m_proxyServer.c_str(),
            m_proxyBypass.empty() ? nullptr : m_proxyBypass.c_str(),
            0
        );

        if (!m_hSession)
        {
            Output::send<LogLevel::Error>(STR("[NetworkClient] WinHttpOpen failed: {}\n"), GetLastError());
            return false;
        }

        m_initialized = true;
        Output::send<LogLevel::Normal>(STR("[NetworkClient] Initialized.\n"));
        return true;
    }

    auto NetworkClient::Cleanup() -> void
    {
        if (m_hSession)
        {
            WinHttpCloseHandle(m_hSession);
            m_hSession = nullptr;
        }
        m_initialized = false;
    }

    auto NetworkClient::ParseUrl(const std::wstring& url) -> ParsedUrl
    {
        ParsedUrl result;

        URL_COMPONENTS urlComp = {};
        urlComp.dwStructSize = sizeof(urlComp);

        wchar_t scheme[16] = {};
        wchar_t host[256] = {};
        wchar_t path[1024] = {};

        urlComp.lpszScheme = scheme;
        urlComp.dwSchemeLength = 16;
        urlComp.lpszHostName = host;
        urlComp.dwHostNameLength = 256;
        urlComp.lpszUrlPath = path;
        urlComp.dwUrlPathLength = 1024;

        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &urlComp))
        {
            Output::send<LogLevel::Error>(STR("[NetworkClient] Failed to parse URL: {}\n"), url);
            return result;
        }

        result.Scheme = scheme;
        result.Host = host;
        result.Path = path;
        result.Port = urlComp.nPort;
        result.IsHttps = (result.Scheme == L"https");

        return result;
    }

    auto NetworkClient::Post(
        const std::wstring& url,
        const std::wstring& requestBody,
        const std::wstring& contentType,
        const std::wstring& authorization,
        int timeoutMs
    ) -> HttpRequestResult
    {
        auto startTime = std::chrono::steady_clock::now();

        HttpRequestResult result;

        if (!m_initialized)
        {
            result.ErrorMessage = L"NetworkClient not initialized";
            return result;
        }

        auto parsedUrl = ParseUrl(url);
        if (parsedUrl.Host.empty())
        {
            result.ErrorMessage = L"Invalid URL";
            return result;
        }

        return PerformRequest(parsedUrl, L"POST", requestBody, contentType, authorization, timeoutMs);
    }

    auto NetworkClient::PerformRequest(
        const ParsedUrl& parsedUrl,
        const std::wstring& method,
        const std::wstring& requestBody,
        const std::wstring& contentType,
        const std::wstring& authorization,
        int timeoutMs
    ) -> HttpRequestResult
    {
        auto startTime = std::chrono::steady_clock::now();
        HttpRequestResult result;

        HINTERNET hConnect = nullptr;
        HINTERNET hRequest = nullptr;
        DWORD flags = 0;

        do
        {
            hConnect = WinHttpConnect(
                static_cast<HINTERNET>(m_hSession),
                parsedUrl.Host.c_str(),
                parsedUrl.Port,
                0
            );

            if (!hConnect)
            {
                result.ErrorMessage = L"Failed to connect to server";
                result.StatusCode = 0;
                break;
            }

            flags = parsedUrl.IsHttps ? WINHTTP_FLAG_SECURE : 0;
            hRequest = WinHttpOpenRequest(
                hConnect,
                method.c_str(),
                parsedUrl.Path.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                flags
            );

            if (!hRequest)
            {
                result.ErrorMessage = L"Failed to create request";
                result.StatusCode = 0;
                break;
            }

            DWORD timeout = timeoutMs;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
            WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
            WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

            if (parsedUrl.IsHttps)
            {
                DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                                SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                                SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
                WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
            }

            std::wstring headers = L"Content-Type: " + contentType + L"\r\n";
            if (!authorization.empty())
            {
                headers += L"Authorization: " + authorization + L"\r\n";
            }

            std::string requestBodyUtf8 = WideToUtf8(requestBody);

            BOOL bResults = WinHttpSendRequest(
                hRequest,
                headers.c_str(),
                static_cast<DWORD>(headers.length()),
                requestBodyUtf8.empty() ? nullptr : const_cast<char*>(requestBodyUtf8.c_str()),
                static_cast<DWORD>(requestBodyUtf8.length()),
                static_cast<DWORD>(requestBodyUtf8.length()),
                0
            );

            if (!bResults)
            {
                result.ErrorMessage = L"Failed to send request";
                result.StatusCode = GetLastError();
                break;
            }

            bResults = WinHttpReceiveResponse(hRequest, nullptr);
            if (!bResults)
            {
                result.ErrorMessage = L"Failed to receive response";
                result.StatusCode = GetLastError();
                break;
            }

            DWORD statusCodeSize = sizeof(DWORD);
            WinHttpQueryHeaders(hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &result.StatusCode,
                &statusCodeSize,
                WINHTTP_NO_HEADER_INDEX);

            DWORD bytesAvailable = 0;
            DWORD bytesRead = 0;
            std::vector<char> responseBuffer;

            do
            {
                bytesAvailable = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable))
                    break;

                if (bytesAvailable == 0)
                    break;

                std::vector<char> tempBuffer(bytesAvailable + 1);
                if (!WinHttpReadData(hRequest, tempBuffer.data(), bytesAvailable, &bytesRead))
                    break;

                responseBuffer.insert(responseBuffer.end(), tempBuffer.begin(), tempBuffer.begin() + bytesRead);
            } while (bytesAvailable > 0);

            if (!responseBuffer.empty())
            {
                responseBuffer.push_back('\0');
                result.ResponseBody = Utf8ToWide(std::string(responseBuffer.data()));
            }

            result.Success = (result.StatusCode >= 200 && result.StatusCode < 300);

        } while (false);

        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);

        auto endTime = std::chrono::steady_clock::now();
        result.Duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        if (!result.Success)
        {
            Output::send<LogLevel::Warning>(STR("[NetworkClient] Request failed: {} (Status: {})\n"),
                result.ErrorMessage, result.StatusCode);
        }

        return result;
    }

    auto NetworkClient::SetProxy(const std::wstring& proxyServer, const std::wstring& proxyBypass) -> void
    {
        m_proxyServer = proxyServer;
        m_proxyBypass = proxyBypass;

        if (m_initialized)
        {
            Cleanup();
            Initialize();
        }
    }

    auto NetworkClient::ClearProxy() -> void
    {
        m_proxyServer.clear();
        m_proxyBypass.clear();

        if (m_initialized)
        {
            Cleanup();
            Initialize();
        }
    }
}