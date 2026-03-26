#pragma once

#define NOMINMAX
#include <windows.h>

#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace RC::RealtimeTranslation
{
    class DebugLogger
    {
    public:
        static DebugLogger& Instance()
        {
            static DebugLogger instance;
            return instance;
        }

        void Initialize(const std::wstring& logPath)
        {
            m_logPath = logPath;
            m_initialized = true;
            Log(L"DebugLogger initialized");
        }

        static std::wstring SanitizeForLog(const std::wstring& text)
        {
            std::wstring result;
            result.reserve(text.size());
            for (auto c : text)
            {
                switch (c)
                {
                    case L'\n': result += L"\\n"; break;
                    case L'\r': result += L"\\r"; break;
                    case L'\t': result += L"\\t"; break;
                    case L'\0': result += L"\\0"; break;
                    default:
                        if (c >= 32 && c < 127)
                        {
                            result += c;
                        }
                        else if (c >= 0x4E00 && c <= 0x9FFF)
                        {
                            result += c;
                        }
                        else if (c >= 0x3040 && c <= 0x30FF)
                        {
                            result += c;
                        }
                        else if (c >= 0xAC00 && c <= 0xD7AF)
                        {
                            result += c;
                        }
                        else
                        {
                            wchar_t buf[16];
                            swprintf_s(buf, L"\\u%04X", (unsigned int)c);
                            result += buf;
                        }
                        break;
                }
            }
            return result;
        }

        void Log(const std::wstring& message)
        {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
            localtime_s(&tm, &time);

            std::wostringstream oss;
            oss << std::put_time(&tm, L"[%H:%M:%S] ");
            oss << SanitizeForLog(message) << L"\n";

            std::wstring fullMessage = oss.str();

            OutputDebugStringW(fullMessage.c_str());

            if (m_initialized)
            {
                std::wofstream file(m_logPath, std::ios::app);
                if (file.is_open())
                {
                    file << fullMessage;
                    file.flush();
                    file.close();
                }
            }
        }

        void Log(const char* message)
        {
            if (!message) return;
            std::wstring wmsg(message, message + strlen(message));
            Log(wmsg);
        }

        void Log(const std::string& message)
        {
            std::wstring wmsg(message.begin(), message.end());
            Log(wmsg);
        }

    private:
        DebugLogger() = default;
        std::wstring m_logPath;
        bool m_initialized = false;
    };

    #define RT_DEBUG_LOG(msg) DebugLogger::Instance().Log(msg)
}
