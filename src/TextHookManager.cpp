#include <TextHookManager.hpp>
#include <HookTargetSelector.hpp>
#include <VocabularyCache.hpp>
#include <TranslationManager.hpp>
#include <ConfigManager.hpp>
#include <DynamicOutput/DynamicOutput.hpp>
#include <DebugLogger.hpp>

#include <Unreal/Hooks/Hooks.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/FText.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/Property/FTextProperty.hpp>
#include <Unreal/CoreUObject/UObject/FStrProperty.hpp>
#include <Unreal/Property/FNameProperty.hpp>
#include <Unreal/Property/FObjectProperty.hpp>

#include <regex>
#include <chrono>

namespace RC::RealtimeTranslation
{
    TextHookManager& TextHookManager::Instance()
    {
        static TextHookManager instance;
        return instance;
    }

    auto TextHookManager::Initialize() -> bool
    {
        if (m_initialized) return true;

        RT_DEBUG_LOG(L"[TextHookManager] Initialize: Starting...");

        RT_DEBUG_LOG(L"[TextHookManager] Initialize: Initializing HookTargetSelector...");
        HookTargetSelector::Instance().Initialize();

        RT_DEBUG_LOG(L"[TextHookManager] Initialize: Registering ProcessEventPreCallback...");
        Unreal::Hook::RegisterProcessEventPreCallback(
            [](Unreal::Hook::TCallbackIterationData<void>& callback_data,
               Unreal::UObject* context,
               Unreal::UFunction* function,
               void* parms) {
                auto& instance = Instance();
                if (!instance.m_hooksEnabled) return;

                auto startTime = std::chrono::high_resolution_clock::now();

                instance.OnProcessEvent(context, function, parms);

                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

                instance.m_totalHookTime += duration.count();
                instance.m_hookCallCount++;
            },
            {false, false, STR("RealtimeTranslationMod"), STR("ProcessEventPre")}
        );

        m_initialized = true;
        RT_DEBUG_LOG(L"[TextHookManager] Initialized.");
        return true;
    }

    auto TextHookManager::Shutdown() -> void
    {
        if (!m_initialized) return;

        DisableHooks();
        m_initialized = false;
        Output::send<LogLevel::Normal>(STR("[TextHookManager] Shutdown complete.\n"));
    }

    auto TextHookManager::RegisterHookCallback(TextHookCallback callback) -> void
    {
        m_callbacks.push_back(std::move(callback));
    }

    auto TextHookManager::SetFilterConfig(const HookFilterConfig& config) -> void
    {
        m_filterConfig = config;
    }

    auto TextHookManager::ShouldTranslate(const std::wstring& text) const -> bool
    {
        if (text.length() < m_filterConfig.MinTextLength)
        {
            return false;
        }

        for (const auto& pattern : m_filterConfig.ExcludePatterns)
        {
            try
            {
                std::wregex re(pattern);
                if (std::regex_match(text, re))
                {
                    return false;
                }
            }
            catch (const std::regex_error&)
            {
            }
        }

        try
        {
            std::wregex numberPattern(LR"(^\d+$)");
            if (std::regex_match(text, numberPattern))
            {
                return false;
            }

            std::wregex singleLetterPattern(LR"(^[A-Za-z]$)");
            if (std::regex_match(text, singleLetterPattern))
            {
                return false;
            }

            std::wregex whitespacePattern(LR"(^\s+$)");
            if (std::regex_match(text, whitespacePattern))
            {
                return false;
            }
        }
        catch (const std::regex_error&)
        {
        }

        return true;
    }

    auto TextHookManager::GetHookStats() const -> const std::vector<HookTargetInfo>&
    {
        return HookTargetSelector::Instance().GetTargets();
    }

    auto TextHookManager::GetAverageHookTime() const -> std::chrono::microseconds
    {
        if (m_hookCallCount == 0) return std::chrono::microseconds(0);
        return std::chrono::microseconds(m_totalHookTime / m_hookCallCount);
    }

    auto TextHookManager::EnableHooks() -> void
    {
        m_hooksEnabled = true;
        Output::send<LogLevel::Normal>(STR("[TextHookManager] Hooks enabled.\n"));
    }

    auto TextHookManager::DisableHooks() -> void
    {
        m_hooksEnabled = false;
        Output::send<LogLevel::Normal>(STR("[TextHookManager] Hooks disabled.\n"));
    }

    auto TextHookManager::OnProcessEvent(
        Unreal::UObject* context,
        Unreal::UFunction* function,
        void* parms
    ) -> void
    {
        if (!function || !context) return;

        auto functionName = function->GetName();
        if (functionName.empty()) return;

        std::string funcNameStr(functionName.begin(), functionName.end());

        static std::atomic<size_t> s_debugLogCount{0};
        if (s_debugLogCount < 500)
        {
            s_debugLogCount++;
            RT_DEBUG_LOG(std::wstring(L"[TextHookManager] ProcessEvent: ") + functionName + L" (" + context->GetName().c_str() + L")");
        }

        bool isTextRelated = false;

        static const std::vector<std::string> textPatterns = {
            "SetText", "K2_SetText", "SetTextContent",
            "SetContent", "SetTextData", "SetDisplayText",
            "SetTextValue", "UpdateText", "RefreshText",
            "SetLabelText", "SetCaption", "SetHint",
            "SetDescription", "SetTitle", "SetMessage",
            "SetTooltip", "SetPlaceholder", "SetHintText",
            "SetDefaultText", "SetRichText", "SetMarkup",
            "FText", "TextBlock", "RichTextBlock", "EditableText",
            "TextProperty", "GetText", "GetDisplayText"
        };

        for (const auto& pattern : textPatterns)
        {
            if (funcNameStr.find(pattern) != std::string::npos)
            {
                isTextRelated = true;
                break;
            }
        }

        if (!isTextRelated) return;

        RT_DEBUG_LOG(std::wstring(L"[TextHookManager] Text-related function detected: ") + functionName);
        HandleSetTextCall(context, function, parms);
    }

    auto TextHookManager::HandleSetTextCall(
        Unreal::UObject* context,
        Unreal::UFunction* function,
        void* parms
    ) -> void
    {
        auto textOpt = ExtractTextFromParms(function, parms);
        if (!textOpt.has_value())
        {
            RT_DEBUG_LOG(L"[TextHookManager] No text extracted from parms");
            return;
        }

        const auto& originalText = textOpt.value();
        RT_DEBUG_LOG(std::wstring(L"[TextHookManager] Extracted text: '") + originalText + L"'");

        if (!ShouldTranslate(originalText))
        {
            RT_DEBUG_LOG(std::wstring(L"[TextHookManager] Text filtered: '") + originalText + L"'");
            return;
        }

        auto& vocabCache = VocabularyCache::Instance();
        std::wstring translatedText;
        bool hasVocabMatch = false;

        if (vocabCache.Lookup(originalText, translatedText))
        {
            hasVocabMatch = true;
            RT_DEBUG_LOG(std::wstring(L"[TextHookManager] Vocabulary hit: '") + originalText + L"' -> '" + translatedText + L"'");
            if (ModifyTextInParms(function, parms, translatedText))
            {
                Output::send<LogLevel::Verbose>(STR("[TextHookManager] Vocabulary hit: '{}' -> '{}'\n"),
                    originalText, translatedText);
            }
        }
        else
        {
            RT_DEBUG_LOG(std::wstring(L"[TextHookManager] Vocabulary miss, enqueuing: '") + originalText + L"'");
        }

        bool shouldBlockOriginal = hasVocabMatch;
        for (const auto& callback : m_callbacks)
        {
            callback(context, originalText, shouldBlockOriginal);
        }

        if (shouldBlockOriginal && !hasVocabMatch)
        {
            Output::send<LogLevel::Verbose>(STR("[TextHookManager] Original call should be blocked (callback requested)\n"));
        }
    }

    auto TextHookManager::ExtractTextFromParms(
        Unreal::UFunction* function,
        void* parms
    ) -> std::optional<std::wstring>
    {
        if (!function || !parms) return std::nullopt;

        RT_DEBUG_LOG(L"[TextHookManager] ExtractTextFromParms: Starting extraction...");

        int propCount = 0;
        for (auto prop = function->GetFirstProperty(); prop; prop = prop->GetNextFieldAsProperty())
        {
            if (!prop) continue;
            propCount++;

            auto propName = prop->GetName();
            auto propClass = prop->GetClass().GetName();
            std::string propClassName(propClass.begin(), propClass.end());
            std::string propNameStr(propName.begin(), propName.end());

            RT_DEBUG_LOG(std::wstring(L"[TextHookManager] Property #") + std::to_wstring(propCount) + 
                         L": Name='" + propName + L"', Class='" + propClass + L"'");

            if (propClassName == "TextProperty" || propClassName == "FTextProperty")
            {
                auto offset = prop->GetOffset_Internal();
                RT_DEBUG_LOG(std::wstring(L"[TextHookManager] TextProperty offset: ") + std::to_wstring(offset));
                auto textPtr = reinterpret_cast<Unreal::FText*>(reinterpret_cast<uintptr_t>(parms) + offset);
                if (textPtr)
                {
                    auto text = textPtr->ToString();
                    if (!text.empty())
                    {
                        RT_DEBUG_LOG(std::wstring(L"[TextHookManager] Found FText: '") + text + L"'");
                        return text;
                    }
                }
            }
            else if (propClassName == "StrProperty" || propClassName == "FStrProperty")
            {
                auto offset = prop->GetOffset_Internal();
                RT_DEBUG_LOG(std::wstring(L"[TextHookManager] StrProperty offset: ") + std::to_wstring(offset));
                auto strPtr = reinterpret_cast<Unreal::FString*>(reinterpret_cast<uintptr_t>(parms) + offset);
                if (strPtr && strPtr->Len() > 0)
                {
                    const wchar_t* strData = **strPtr;
                    if (strData)
                    {
                        std::wstring result(strData);
                        RT_DEBUG_LOG(std::wstring(L"[TextHookManager] Found FString: '") + result + L"'");
                        return result;
                    }
                }
            }
            else if (propClassName == "StructProperty" || propClassName == "FStructProperty")
            {
                auto structProp = static_cast<Unreal::FStructProperty*>(prop);
                auto scriptStruct = structProp->GetStruct();
                if (scriptStruct)
                {
                    auto structName = scriptStruct->GetName();
                    std::string structNameStr(structName.begin(), structName.end());

                    RT_DEBUG_LOG(std::wstring(L"[TextHookManager] StructProperty struct: '") + structName + L"'");

                    if (structNameStr.find("Text") != std::string::npos)
                    {
                        auto offset = structProp->GetOffset_Internal();
                        auto textPtr = reinterpret_cast<Unreal::FText*>(reinterpret_cast<uintptr_t>(parms) + offset);
                        if (textPtr)
                        {
                            auto text = textPtr->ToString();
                            if (!text.empty())
                            {
                                RT_DEBUG_LOG(std::wstring(L"[TextHookManager] Found Text in struct: '") + text + L"'");
                                return text;
                            }
                        }
                    }
                }
            }
            else if (propClassName == "NameProperty" || propClassName == "FNameProperty")
            {
                auto offset = prop->GetOffset_Internal();
                auto namePtr = reinterpret_cast<Unreal::FName*>(reinterpret_cast<uintptr_t>(parms) + offset);
                if (namePtr)
                {
                    auto name = namePtr->ToString();
                    if (!name.empty())
                    {
                        RT_DEBUG_LOG(std::wstring(L"[TextHookManager] Found FName: '") + name + L"'");
                    }
                }
            }
            else if (propClassName == "ObjectProperty" || propClassName == "FObjectProperty")
            {
                auto offset = prop->GetOffset_Internal();
                auto objPtr = reinterpret_cast<Unreal::UObject**>(reinterpret_cast<uintptr_t>(parms) + offset);
                if (objPtr && *objPtr)
                {
                    auto objName = (*objPtr)->GetName();
                    RT_DEBUG_LOG(std::wstring(L"[TextHookManager] Found UObject: '") + objName + L"'");
                }
            }
        }

        RT_DEBUG_LOG(std::wstring(L"[TextHookManager] No text found in ") + std::to_wstring(propCount) + L" properties");
        return std::nullopt;
    }

    auto TextHookManager::ModifyTextInParms(
        Unreal::UFunction* function,
        void* parms,
        const std::wstring& newText
    ) -> bool
    {
        if (!function || !parms) return false;

        for (auto prop = function->GetFirstProperty(); prop; prop = prop->GetNextFieldAsProperty())
        {
            if (!prop) continue;

            auto propClass = prop->GetClass().GetName();
            std::string propClassName(propClass.begin(), propClass.end());

            if (propClassName == "TextProperty" || propClassName == "FTextProperty")
            {
                auto offset = prop->GetOffset_Internal();
                auto textPtr = reinterpret_cast<Unreal::FText*>(reinterpret_cast<uintptr_t>(parms) + offset);
                if (textPtr)
                {
                    textPtr->SetString(Unreal::FString(newText.c_str()));
                    return true;
                }
            }
            else if (propClassName == "StructProperty" || propClassName == "FStructProperty")
            {
                auto structProp = static_cast<Unreal::FStructProperty*>(prop);
                auto scriptStruct = structProp->GetStruct();
                if (scriptStruct)
                {
                    auto structName = scriptStruct->GetName();
                    std::string structNameStr(structName.begin(), structName.end());

                    if (structNameStr.find("Text") != std::string::npos)
                    {
                        auto offset = structProp->GetOffset_Internal();
                        auto textPtr = reinterpret_cast<Unreal::FText*>(reinterpret_cast<uintptr_t>(parms) + offset);
                        if (textPtr)
                        {
                            textPtr->SetString(Unreal::FString(newText.c_str()));
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }
}