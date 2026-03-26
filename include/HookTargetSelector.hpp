#pragma once

#include <string>
#include <vector>
#include <optional>

namespace RC::RealtimeTranslation
{
    struct HookTargetInfo
    {
        std::string FunctionName;
        std::string ClassName;
        int Priority;
        bool IsAvailable;
        std::wstring StatusMessage;
    };

    class HookTargetSelector
    {
    public:
        static HookTargetSelector& Instance();

        auto Initialize() -> void;
        [[nodiscard]] auto GetTargets() const -> const std::vector<HookTargetInfo>& { return m_targets; }
        auto GetAvailableTargets() const -> std::vector<HookTargetInfo>;
        auto DetectTarget(const std::string& functionName) -> bool;
        auto GetBestTarget() const -> std::optional<HookTargetInfo>;

    private:
        HookTargetSelector() = default;
        ~HookTargetSelector() = default;
        HookTargetSelector(const HookTargetSelector&) = delete;
        HookTargetSelector& operator=(const HookTargetSelector&) = delete;

        auto DetectUTextBlockSetText() -> bool;
        auto DetectURichTextBlockSetText() -> bool;
        auto DetectUEditableTextSetText() -> bool;
        auto DetectFTextToString() -> bool;

        std::vector<HookTargetInfo> m_targets;
        bool m_initialized = false;
    };
}
