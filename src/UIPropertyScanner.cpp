#include <UIPropertyScanner.hpp>
#include <VocabularyCache.hpp>
#include <TranslationManager.hpp>
#include <DebugLogger.hpp>

#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/Property/FTextProperty.hpp>
#include <Unreal/Core/GenericPlatform/GenericPlatform.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

namespace RC::RealtimeTranslation
{
    UIPropertyScanner& UIPropertyScanner::Instance()
    {
        static UIPropertyScanner instance;
        return instance;
    }

    auto UIPropertyScanner::Initialize() -> void
    {
        if (m_initialized) return;

        RT_DEBUG_LOG(L"[UIPropertyScanner] Initialized.");
        m_initialized = true;
    }

    auto UIPropertyScanner::Shutdown() -> void
    {
        StopScanning();
        m_initialized = false;
    }

    auto UIPropertyScanner::StartScanning() -> void
    {
        if (!m_initialized || m_scanning.load()) return;

        m_scannedObjects.clear();
        m_scanning = true;
        m_lastScanTime = std::chrono::steady_clock::now();

        RT_DEBUG_LOG(L"[UIPropertyScanner] Scanning started.");
    }

    auto UIPropertyScanner::StopScanning() -> void
    {
        m_scanning = false;
        RT_DEBUG_LOG(L"[UIPropertyScanner] Scanning stopped.");
    }

    auto UIPropertyScanner::SetTranslationCallback(TranslationCallback callback) -> void
    {
        m_translationCallback = std::move(callback);
    }

    auto UIPropertyScanner::ScanAllWidgets() -> void
    {
        if (!m_scanning.load()) return;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastScanTime);
        if (elapsed < ScanInterval)
        {
            return;
        }
        m_lastScanTime = now;

        size_t widgetCount = 0;
        size_t textFound = 0;

        Unreal::UObjectGlobals::ForEachUObject([&](Unreal::UObject* object, auto, auto) -> LoopAction {
            if (!object) return LoopAction::Continue;

            auto* classObj = object->GetClassPrivate();
            if (!classObj) return LoopAction::Continue;

            auto className = classObj->GetName();
            std::wstring classNameStr(className.begin(), className.end());

            bool isWidget = false;
            if (classNameStr.find(L"WBP_") == 0 ||
                classNameStr.find(L"WBPHUD_") == 0 ||
                classNameStr.find(L"UMG_") == 0 ||
                classNameStr.find(L"TextBlock") != std::wstring::npos ||
                classNameStr.find(L"RichTextBlock") != std::wstring::npos ||
                classNameStr.find(L"EditableText") != std::wstring::npos ||
                classNameStr.find(L"UserWidget") != std::wstring::npos ||
                classNameStr.find(L"Widget") != std::wstring::npos)
            {
                isWidget = true;
            }

            if (!isWidget) return LoopAction::Continue;

            widgetCount++;

            uintptr_t objAddr = reinterpret_cast<uintptr_t>(object);
            if (m_scannedObjects.contains(objAddr))
            {
                return LoopAction::Continue;
            }

            if (m_scannedObjects.size() < MaxScannedObjects)
            {
                m_scannedObjects.insert(objAddr);
            }

            if (ScanWidgetProperties(object))
            {
                textFound++;
            }

            return LoopAction::Continue;
        });

        static std::atomic<size_t> s_scanLogCount{0};
        if (s_scanLogCount < 10)
        {
            s_scanLogCount++;
            RT_DEBUG_LOG(std::wstring(L"[UIPropertyScanner] Scanned ") + std::to_wstring(widgetCount) + L" widgets, found " + std::to_wstring(textFound) + L" with text");
        }
    }

    auto UIPropertyScanner::ScanWidgetProperties(Unreal::UObject* widget) -> bool
    {
        if (!widget) return false;

        auto* classObj = widget->GetClassPrivate();
        if (!classObj) return false;

        auto className = classObj->GetName();
        std::wstring classNameStr(className.begin(), className.end());

        bool foundText = false;

        for (auto* prop = classObj->GetFirstProperty(); prop; prop = prop->GetNextFieldAsProperty())
        {
            if (!prop) continue;

            if (!IsTextProperty(prop)) continue;

            auto propName = prop->GetName();
            auto offset = prop->GetOffset_Internal();

            void* data = reinterpret_cast<uint8_t*>(widget) + offset;

            auto textOpt = ExtractTextFromProperty(prop, data);
            if (!textOpt.has_value()) continue;

            const auto& text = textOpt.value();

            if (text.length() < 2) continue;
            if (text.length() > 500) continue;

            static std::atomic<size_t> s_textLogCount{0};
            if (s_textLogCount < 100)
            {
                s_textLogCount++;
                RT_DEBUG_LOG(std::wstring(L"[UIPropertyScanner] Found text in ") + classNameStr + L"." + propName + L": '" + text + L"'");
            }

            foundText = true;

            if (m_translationCallback)
            {
                m_translationCallback(text, widget, prop, offset);
            }
        }

        return foundText;
    }

    auto UIPropertyScanner::IsTextProperty(Unreal::FProperty* prop) -> bool
    {
        if (!prop) return false;

        auto propClass = prop->GetClass().GetName();
        std::wstring propClassName(propClass.begin(), propClass.end());

        if (propClassName == L"TextProperty" || propClassName == L"FTextProperty" ||
            propClassName == L"StrProperty" || propClassName == L"FStrProperty")
        {
            auto propName = prop->GetName();
            std::wstring nameLower(propName.begin(), propName.end());

            if (nameLower.find(L"Text") != std::wstring::npos ||
                nameLower.find(L"Label") != std::wstring::npos ||
                nameLower.find(L"Title") != std::wstring::npos ||
                nameLower.find(L"Description") != std::wstring::npos ||
                nameLower.find(L"Message") != std::wstring::npos ||
                nameLower.find(L"Tooltip") != std::wstring::npos ||
                nameLower.find(L"Name") == 0 ||
                nameLower.find(L"Header") != std::wstring::npos ||
                nameLower.find(L"Content") != std::wstring::npos ||
                nameLower.find(L"Caption") != std::wstring::npos ||
                nameLower.find(L"Hint") != std::wstring::npos ||
                nameLower.find(L"Display") != std::wstring::npos)
            {
                return true;
            }
        }

        return false;
    }

    auto UIPropertyScanner::ExtractTextFromProperty(Unreal::FProperty* prop, void* data) -> std::optional<std::wstring>
    {
        return GetTextPropertyValue(prop, data);
    }

    auto UIPropertyScanner::GetTextPropertyValue(Unreal::FProperty* prop, void* data) -> std::optional<std::wstring>
    {
        if (!prop || !data) return std::nullopt;

        auto propClass = prop->GetClass().GetName();
        std::wstring propClassName(propClass.begin(), propClass.end());

        if (propClassName == L"TextProperty" || propClassName == L"FTextProperty")
        {
            auto* textPtr = reinterpret_cast<Unreal::FText*>(data);
            if (textPtr)
            {
                auto text = textPtr->ToString();
                if (!text.empty())
                {
                    return text;
                }
            }
        }
        else if (propClassName == L"StrProperty" || propClassName == L"FStrProperty")
        {
            auto* strPtr = reinterpret_cast<Unreal::FString*>(data);
            if (strPtr && strPtr->Len() > 0)
            {
                const wchar_t* strData = **strPtr;
                if (strData)
                {
                    return std::wstring(strData);
                }
            }
        }

        return std::nullopt;
    }

    auto UIPropertyScanner::SetTextPropertyValue(Unreal::FProperty* prop, void* data, const std::wstring& newText) -> bool
    {
        if (!prop || !data) return false;

        auto propClass = prop->GetClass().GetName();
        std::wstring propClassName(propClass.begin(), propClass.end());

        if (propClassName == L"TextProperty" || propClassName == L"FTextProperty")
        {
            auto* textPtr = reinterpret_cast<Unreal::FText*>(data);
            if (textPtr)
            {
                textPtr->SetString(Unreal::FString(newText.c_str()));
                return true;
            }
        }
        else if (propClassName == L"StrProperty" || propClassName == L"FStrProperty")
        {
            auto* strPtr = reinterpret_cast<Unreal::FString*>(data);
            if (strPtr)
            {
                *strPtr = Unreal::FString(newText.c_str());
                return true;
            }
        }

        return false;
    }
}
