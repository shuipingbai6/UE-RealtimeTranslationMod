#include <HookTargetSelector.hpp>
#include <DynamicOutput/DynamicOutput.hpp>
#include <DebugLogger.hpp>

#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>

namespace RC::RealtimeTranslation
{
    HookTargetSelector& HookTargetSelector::Instance()
    {
        static HookTargetSelector instance;
        return instance;
    }

    auto HookTargetSelector::Initialize() -> void
    {
        if (m_initialized) return;

        RT_DEBUG_LOG(L"[HookTargetSelector] Initialize: Starting...");

        m_targets = {
            {"UTextBlock::SetText", "UTextBlock", 1, false, L"Not checked"},
            {"URichTextBlock::SetText", "URichTextBlock", 2, false, L"Not checked"},
            {"UEditableText::SetText", "UEditableText", 3, false, L"Not checked"},
            {"FText::ToString", "FText", 4, false, L"Not checked"}
        };

        RT_DEBUG_LOG(L"[HookTargetSelector] Initialize: Detecting UTextBlock::SetText...");
        DetectUTextBlockSetText();
        RT_DEBUG_LOG(L"[HookTargetSelector] Initialize: Detecting URichTextBlock::SetText...");
        DetectURichTextBlockSetText();
        RT_DEBUG_LOG(L"[HookTargetSelector] Initialize: Detecting UEditableText::SetText...");
        DetectUEditableTextSetText();
        RT_DEBUG_LOG(L"[HookTargetSelector] Initialize: Detecting FText::ToString...");
        DetectFTextToString();

        m_initialized = true;

        RT_DEBUG_LOG(L"[HookTargetSelector] Hook target detection complete.");
        for (const auto& target : m_targets)
        {
            auto status = target.IsAvailable ? L"Available" : L"Not available";
            std::wstring msg = L"[HookTargetSelector] " + to_wstring(target.FunctionName) + L" - " + status + L" (" + target.StatusMessage + L")";
            RT_DEBUG_LOG(msg);
        }
    }

    auto HookTargetSelector::GetAvailableTargets() const -> std::vector<HookTargetInfo>
    {
        std::vector<HookTargetInfo> available;
        for (const auto& target : m_targets)
        {
            if (target.IsAvailable)
            {
                available.push_back(target);
            }
        }
        return available;
    }

    auto HookTargetSelector::DetectTarget(const std::string& functionName) -> bool
    {
        for (const auto& target : m_targets)
        {
            if (target.FunctionName == functionName)
            {
                return target.IsAvailable;
            }
        }
        return false;
    }

    auto HookTargetSelector::GetBestTarget() const -> std::optional<HookTargetInfo>
    {
        for (const auto& target : m_targets)
        {
            if (target.IsAvailable)
            {
                return target;
            }
        }
        return std::nullopt;
    }

    auto HookTargetSelector::DetectUTextBlockSetText() -> bool
    {
        auto& target = m_targets[0];

        try
        {
            RT_DEBUG_LOG(L"[HookTargetSelector] DetectUTextBlockSetText: Calling StaticFindObject...");

            auto textBlockClass = Unreal::UObjectGlobals::StaticFindObject<Unreal::UClass*>(
                nullptr,
                nullptr,
                STR("/Script/UMG.TextBlock")
            );

            RT_DEBUG_LOG(L"[HookTargetSelector] DetectUTextBlockSetText: StaticFindObject returned.");

            if (textBlockClass)
            {
                RT_DEBUG_LOG(L"[HookTargetSelector] DetectUTextBlockSetText: Class found, getting function...");
                auto setTextInputFn = textBlockClass->GetFunctionByName(STR("SetText"));
                if (setTextInputFn)
                {
                    target.IsAvailable = true;
                    target.StatusMessage = L"Found via class lookup";
                    RT_DEBUG_LOG(L"[HookTargetSelector] UTextBlock::SetText found.");
                    return true;
                }
            }

            target.StatusMessage = L"Class or function not found";
            RT_DEBUG_LOG(L"[HookTargetSelector] UTextBlock::SetText not found.");
            return false;
        }
        catch (const std::exception& e)
        {
            target.StatusMessage = L"Exception during detection";
            std::string msg = std::string("[HookTargetSelector] Exception in DetectUTextBlockSetText: ") + e.what();
            RT_DEBUG_LOG(msg.c_str());
            return false;
        }
        catch (...)
        {
            target.StatusMessage = L"Exception during detection";
            RT_DEBUG_LOG(L"[HookTargetSelector] Unknown exception in DetectUTextBlockSetText.");
            return false;
        }
    }

    auto HookTargetSelector::DetectURichTextBlockSetText() -> bool
    {
        auto& target = m_targets[1];

        try
        {
            RT_DEBUG_LOG(L"[HookTargetSelector] DetectURichTextBlockSetText: Calling StaticFindObject...");

            auto richTextClass = Unreal::UObjectGlobals::StaticFindObject<Unreal::UClass*>(
                nullptr,
                nullptr,
                STR("/Script/UMG.RichTextBlock")
            );

            RT_DEBUG_LOG(L"[HookTargetSelector] DetectURichTextBlockSetText: StaticFindObject returned.");

            if (richTextClass)
            {
                RT_DEBUG_LOG(L"[HookTargetSelector] DetectURichTextBlockSetText: Class found, getting function...");
                auto setTextInputFn = richTextClass->GetFunctionByName(STR("SetText"));
                if (setTextInputFn)
                {
                    target.IsAvailable = true;
                    target.StatusMessage = L"Found via class lookup";
                    RT_DEBUG_LOG(L"[HookTargetSelector] URichTextBlock::SetText found.");
                    return true;
                }
            }

            target.StatusMessage = L"Class or function not found";
            RT_DEBUG_LOG(L"[HookTargetSelector] URichTextBlock::SetText not found.");
            return false;
        }
        catch (const std::exception& e)
        {
            target.StatusMessage = L"Exception during detection";
            std::string msg = std::string("[HookTargetSelector] Exception in DetectURichTextBlockSetText: ") + e.what();
            RT_DEBUG_LOG(msg.c_str());
            return false;
        }
        catch (...)
        {
            target.StatusMessage = L"Exception during detection";
            RT_DEBUG_LOG(L"[HookTargetSelector] Unknown exception in DetectURichTextBlockSetText.");
            return false;
        }
    }

    auto HookTargetSelector::DetectUEditableTextSetText() -> bool
    {
        auto& target = m_targets[2];

        try
        {
            RT_DEBUG_LOG(L"[HookTargetSelector] DetectUEditableTextSetText: Calling StaticFindObject...");

            auto editableTextClass = Unreal::UObjectGlobals::StaticFindObject<Unreal::UClass*>(
                nullptr,
                nullptr,
                STR("/Script/UMG.EditableText")
            );

            RT_DEBUG_LOG(L"[HookTargetSelector] DetectUEditableTextSetText: StaticFindObject returned.");

            if (editableTextClass)
            {
                RT_DEBUG_LOG(L"[HookTargetSelector] DetectUEditableTextSetText: Class found, getting function...");
                auto setTextInputFn = editableTextClass->GetFunctionByName(STR("SetText"));
                if (setTextInputFn)
                {
                    target.IsAvailable = true;
                    target.StatusMessage = L"Found via class lookup";
                    RT_DEBUG_LOG(L"[HookTargetSelector] UEditableText::SetText found.");
                    return true;
                }
            }

            target.StatusMessage = L"Class or function not found";
            RT_DEBUG_LOG(L"[HookTargetSelector] UEditableText::SetText not found.");
            return false;
        }
        catch (const std::exception& e)
        {
            target.StatusMessage = L"Exception during detection";
            std::string msg = std::string("[HookTargetSelector] Exception in DetectUEditableTextSetText: ") + e.what();
            RT_DEBUG_LOG(msg.c_str());
            return false;
        }
        catch (...)
        {
            target.StatusMessage = L"Exception during detection";
            RT_DEBUG_LOG(L"[HookTargetSelector] Unknown exception in DetectUEditableTextSetText.");
            return false;
        }
    }

    auto HookTargetSelector::DetectFTextToString() -> bool
    {
        auto& target = m_targets[3];

        target.IsAvailable = true;
        target.StatusMessage = L"Always available (fallback)";
        RT_DEBUG_LOG(L"[HookTargetSelector] FText::ToString always available.");
        return true;
    }
}