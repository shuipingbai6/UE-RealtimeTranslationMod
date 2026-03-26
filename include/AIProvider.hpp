#pragma once

#include "TranslationCommon.hpp"
#include "NetworkClient.hpp"
#include <functional>
#include <memory>
#include <chrono>
#include <mutex>

namespace RC::RealtimeTranslation
{
    class AIProvider
    {
    public:
        static AIProvider& Instance();

        auto Initialize(const AIProviderConfig& config) -> bool;

        auto Translate(
            const std::wstring& text,
            const std::wstring& sourceLang = L"auto",
            const std::wstring& targetLang = L"zh-CN"
        ) -> TranslationResult;

        auto TranslateBatch(
            const std::vector<std::wstring>& texts,
            const std::wstring& sourceLang = L"auto",
            const std::wstring& targetLang = L"zh-CN"
        ) -> std::vector<TranslationResult>;

        auto UpdateConfig(const AIProviderConfig& config) -> void;

        [[nodiscard]] auto GetConfig() const -> const AIProviderConfig& { return m_config; }

        [[nodiscard]] auto IsConfigured() const -> bool;

        auto TestConnection() -> std::pair<bool, std::wstring>;

    private:
        AIProvider() = default;
        ~AIProvider() = default;
        AIProvider(const AIProvider&) = delete;
        AIProvider& operator=(const AIProvider&) = delete;

        auto BuildTranslationRequest(
            const std::wstring& text,
            const std::wstring& sourceLang,
            const std::wstring& targetLang
        ) -> std::wstring;

        auto ParseTranslationResponse(const std::wstring& response) -> std::wstring;

        auto BuildTranslationPrompt(
            const std::wstring& text,
            const std::wstring& sourceLang,
            const std::wstring& targetLang
        ) -> std::wstring;

        auto WaitForRateLimit() -> void;

        AIProviderConfig m_config;
        mutable std::shared_mutex m_mutex;
        bool m_initialized{false};

        std::chrono::steady_clock::time_point m_lastRequestTime;
        std::mutex m_rateLimitMutex;
    };
}
