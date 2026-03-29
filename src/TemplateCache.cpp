#include <TemplateCache.hpp>
#include <DebugLogger.hpp>
#include <DynamicOutput/DynamicOutput.hpp>

namespace RC::RealtimeTranslation
{
    TemplateCache& TemplateCache::Instance()
    {
        static TemplateCache instance;
        return instance;
    }

    auto TemplateCache::Initialize(const std::wstring& filePath) -> bool
    {
        std::unique_lock lock(m_mutex);
        m_filePath = filePath;
        m_initialized = true;

        RT_DEBUG_LOG(std::wstring(L"[TemplateCache] Initialized with file: ") + filePath);
        return true;
    }

    auto TemplateCache::GetTemplate(const std::wstring& skeleton) -> std::optional<TemplateEntry>
    {
        m_queryCount++;

        std::shared_lock lock(m_mutex);
        auto it = m_templates.find(skeleton);
        if (it != m_templates.end())
        {
            m_hitCount++;
            RT_DEBUG_LOG(std::wstring(L"[TemplateCache] Cache hit for skeleton: '") + skeleton + L"'");
            return it->second;
        }

        RT_DEBUG_LOG(std::wstring(L"[TemplateCache] Cache miss for skeleton: '") + skeleton + L"'");
        return std::nullopt;
    }

    auto TemplateCache::LookupTemplate(const std::wstring& skeleton, TemplateEntry& outEntry) -> bool
    {
        m_queryCount++;

        std::shared_lock lock(m_mutex);
        auto it = m_templates.find(skeleton);
        if (it != m_templates.end())
        {
            m_hitCount++;
            outEntry = it->second;
            RT_DEBUG_LOG(std::wstring(L"[TemplateCache] Lookup hit for skeleton: '") + skeleton + L"'");
            return true;
        }

        return false;
    }

    auto TemplateCache::StoreTemplate(const std::wstring& sourceSkeleton, 
                                       const std::wstring& targetTemplate,
                                       const std::vector<EntityType>& entityTypes) -> void
    {
        {
            std::unique_lock lock(m_mutex);
            TemplateEntry entry;
            entry.SourceSkeleton = sourceSkeleton;
            entry.TargetTemplate = targetTemplate;
            entry.EntityTypes = entityTypes;
            m_templates[sourceSkeleton] = entry;
        }
        m_dirty = true;

        RT_DEBUG_LOG(std::wstring(L"[TemplateCache] Stored template: '") + sourceSkeleton + 
            L"' -> '" + targetTemplate + L"'");
    }

    auto TemplateCache::StoreFromTranslation(const ExtractionResult& extraction,
                                              const std::wstring& translatedText) -> void
    {
        if (extraction.Entities.empty())
        {
            // 没有实体，不需要存储模板
            return;
        }

        // 收集实体类型顺序
        std::vector<EntityType> entityTypes;
        for (const auto& entity : extraction.Entities)
        {
            entityTypes.push_back(entity.Type);
        }

        StoreTemplate(extraction.SkeletonText, translatedText, entityTypes);
    }

    auto TemplateCache::Load() -> bool
    {
        // 模板数据由 VocabularyCache 统一加载
        // 此处仅做标记
        RT_DEBUG_LOG(L"[TemplateCache] Load called (handled by VocabularyCache)");
        return true;
    }

    auto TemplateCache::Save() -> bool
    {
        // 模板数据由 VocabularyCache 统一保存
        // 此处仅做标记
        RT_DEBUG_LOG(L"[TemplateCache] Save called (handled by VocabularyCache)");
        return true;
    }

    auto TemplateCache::GetTemplateCount() const -> size_t
    {
        std::shared_lock lock(m_mutex);
        return m_templates.size();
    }

    auto TemplateCache::GetHitRate() const -> float
    {
        size_t total = m_queryCount;
        return total > 0 ? static_cast<float>(m_hitCount) / static_cast<float>(total) : 0.0f;
    }

    auto TemplateCache::ResetStats() -> void
    {
        m_hitCount = 0;
        m_queryCount = 0;
    }

    auto TemplateCache::Clear() -> void
    {
        std::unique_lock lock(m_mutex);
        m_templates.clear();
        m_dirty = true;
    }

    auto TemplateCache::ExportAllTemplates() const -> std::vector<TemplateEntry>
    {
        std::shared_lock lock(m_mutex);
        std::vector<TemplateEntry> result;
        result.reserve(m_templates.size());
        for (const auto& [key, entry] : m_templates)
        {
            result.push_back(entry);
        }
        return result;
    }
} // namespace RC::RealtimeTranslation
