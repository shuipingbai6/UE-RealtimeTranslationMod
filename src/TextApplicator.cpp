#include <TextApplicator.hpp>
#include <VocabularyCache.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <Unreal/FText.hpp>
#include <Unreal/FString.hpp>
#include <Unreal/Property/FTextProperty.hpp>

namespace RC::RealtimeTranslation
{
    TextApplicator& TextApplicator::Instance()
    {
        static TextApplicator instance;
        return instance;
    }

    auto TextApplicator::Initialize() -> bool
    {
        if (m_initialized) return true;

        m_initialized = true;
        Output::send<LogLevel::Normal>(STR("[TextApplicator] Initialized.\n"));
        return true;
    }

    auto TextApplicator::Shutdown() -> void
    {
        if (!m_initialized) return;

        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_pendingQueue.empty())
        {
            m_pendingQueue.pop();
        }

        m_initialized = false;
        Output::send<LogLevel::Normal>(STR("[TextApplicator] Shutdown complete.\n"));
    }

    auto TextApplicator::ApplySync(
        const std::wstring& original,
        const std::wstring& translated,
        Unreal::UObject* widget
    ) -> bool
    {
        if (!widget || !ValidateTranslation(original, translated))
        {
            return false;
        }

        return ApplyToWidget(widget, translated);
    }

    auto TextApplicator::ApplyAsync(
        const std::wstring& original,
        const std::wstring& translated,
        void* widgetPtr
    ) -> void
    {
        if (!ValidateTranslation(original, translated))
        {
            return;
        }

        PendingApplication app;
        app.OriginalText = original;
        app.TranslatedText = translated;
        app.WidgetPtr = widgetPtr;
        app.Timestamp = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_pendingQueue.push(std::move(app));
    }

    auto TextApplicator::ProcessPendingApplications() -> void
    {
        std::queue<PendingApplication> localQueue;

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            localQueue = std::move(m_pendingQueue);
            while (!m_pendingQueue.empty())
            {
                m_pendingQueue.pop();
            }
        }

        while (!localQueue.empty())
        {
            auto& app = localQueue.front();

            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - app.Timestamp
            );
            if (age.count() < 1000)
            {
                if (app.WidgetPtr)
                {
                    auto widget = static_cast<Unreal::UObject*>(app.WidgetPtr);
                    ApplyToWidget(widget, app.TranslatedText);
                }
                else
                {
                    RefreshWidgets(app.OriginalText);
                }
            }

            localQueue.pop();
        }
    }

    auto TextApplicator::RefreshWidgets(const std::wstring& originalText) -> void
    {
        auto widgets = FindWidgetsByText(originalText);
        for (auto* widget : widgets)
        {
            auto translated = VocabularyCache::Instance().Get(originalText);
            if (translated.has_value())
            {
                ApplyToWidget(widget, translated.value());
            }
        }
    }

    auto TextApplicator::ValidateTranslation(
        const std::wstring& original,
        const std::wstring& translated
    ) const -> bool
    {
        if (translated.empty())
        {
            return false;
        }

        if (original == translated)
        {
            return false;
        }

        return true;
    }

    auto TextApplicator::SetBatchDelay(int delayMs) -> void
    {
        m_batchDelayMs = delayMs;
    }

    auto TextApplicator::GetPendingCount() const -> size_t
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        return m_pendingQueue.size();
    }

    auto TextApplicator::ApplyToWidget(
        Unreal::UObject* widget,
        const std::wstring& text
    ) -> bool
    {
        if (!widget) return false;

        try
        {
            auto setTextInputFn = widget->GetFunctionByName(STR("SetText"));
            if (setTextInputFn)
            {
                Unreal::FText newText(text.c_str());

                struct SetTextParams
                {
                    Unreal::FText InText;
                };

                SetTextParams params;
                params.InText = newText;

                widget->ProcessEvent(setTextInputFn, &params);
                return true;
            }

            auto k2SetTextInputFn = widget->GetFunctionByName(STR("K2_SetText"));
            if (k2SetTextInputFn)
            {
                Unreal::FText newText(text.c_str());

                struct K2SetTextParams
                {
                    Unreal::FText InText;
                };

                K2SetTextParams params;
                params.InText = newText;

                widget->ProcessEvent(k2SetTextInputFn, &params);
                return true;
            }

            Output::send<LogLevel::Verbose>(STR("[TextApplicator] SetText function not found on widget.\n"));
            return false;
        }
        catch (...)
        {
            Output::send<LogLevel::Warning>(STR("[TextApplicator] Exception while applying text to widget.\n"));
            return false;
        }
    }

    auto TextApplicator::FindWidgetsByText(const std::wstring& text) -> std::vector<Unreal::UObject*>
    {
        std::vector<Unreal::UObject*> result;

        {
            std::shared_lock lock(m_cacheMutex);
            auto it = m_widgetCache.find(text);
            if (it != m_widgetCache.end())
            {
                return it->second;
            }
        }

        return result;
    }
}