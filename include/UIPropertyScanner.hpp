#pragma once

#include "TranslationCommon.hpp"
#include <Unreal/UObject.hpp>
#include <Unreal/FProperty.hpp>
#include <functional>
#include <atomic>
#include <chrono>
#include <unordered_set>

namespace RC::RealtimeTranslation
{
    class UIPropertyScanner
    {
    public:
        static UIPropertyScanner& Instance();

        auto Initialize() -> void;
        auto Shutdown() -> void;

        auto StartScanning() -> void;
        auto StopScanning() -> void;
        auto ClearScannedObjects() -> void { m_scannedObjects.clear(); }
        [[nodiscard]] auto IsScanning() const -> bool { return m_scanning.load(); }

        auto ScanAllWidgets() -> void;

        using TranslationCallback = std::function<void(const std::wstring&, Unreal::UObject*, Unreal::FProperty*, size_t)>;
        auto SetTranslationCallback(TranslationCallback callback) -> void;

        auto SetTextPropertyValue(Unreal::FProperty* prop, void* data, const std::wstring& newText) -> bool;

    private:
        UIPropertyScanner() = default;
        ~UIPropertyScanner() = default;
        UIPropertyScanner(const UIPropertyScanner&) = delete;
        UIPropertyScanner& operator=(const UIPropertyScanner&) = delete;

        auto ScanWidgetProperties(Unreal::UObject* widget) -> bool;
        auto ExtractTextFromProperty(Unreal::FProperty* prop, void* data) -> std::optional<std::wstring>;
        auto GetTextPropertyValue(Unreal::FProperty* prop, void* data) -> std::optional<std::wstring>;
        auto IsTextProperty(Unreal::FProperty* prop) -> bool;

        std::atomic<bool> m_initialized{false};
        std::atomic<bool> m_scanning{false};

        TranslationCallback m_translationCallback;

        std::unordered_set<uintptr_t> m_scannedObjects;

        static constexpr size_t MaxScannedObjects = 10000;
        static constexpr auto ScanInterval = std::chrono::milliseconds(100);

        std::chrono::steady_clock::time_point m_lastScanTime;
    };
}
