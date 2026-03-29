#pragma once

#include "TranslationCommon.hpp"
#include "EntityExtractor.hpp"
#include <shared_mutex>
#include <unordered_map>
#include <optional>
#include <vector>

namespace RC::RealtimeTranslation
{
    /**
     * 模板缓存条目
     * 存储骨架文本到翻译模板的映射
     */
    struct TemplateEntry
    {
        std::wstring SourceSkeleton;    // 原始语言骨架（如 {NUM}円を払う）
        std::wstring TargetTemplate;    // 目标语言模板（如 支付{NUM}日元）
        std::vector<EntityType> EntityTypes;  // 实体类型顺序
    };

    /**
     * 模板缓存器
     * 实现骨架文本到翻译模板的缓存，支持持久化存储
     */
    class TemplateCache
    {
    public:
        static TemplateCache& Instance();

        /**
         * 初始化模板缓存
         * @param filePath 词库文件路径（模板数据存储在同一文件中）
         */
        auto Initialize(const std::wstring& filePath) -> bool;

        /**
         * 查询模板缓存
         * @param skeleton 骨架文本
         * @return 翻译模板（如果存在）
         */
        [[nodiscard]] auto GetTemplate(const std::wstring& skeleton) -> std::optional<TemplateEntry>;

        /**
         * 查询模板缓存（输出参数版本）
         * @param skeleton 骨架文本
         * @param outEntry 输出的模板条目
         * @return 是否找到
         */
        [[nodiscard]] auto LookupTemplate(const std::wstring& skeleton, TemplateEntry& outEntry) -> bool;

        /**
         * 存储翻译模板
         * @param sourceSkeleton 原始骨架（如 {NUM}円を払う）
         * @param targetTemplate 目标模板（如 支付{NUM}日元）
         * @param entityTypes 实体类型顺序
         */
        auto StoreTemplate(const std::wstring& sourceSkeleton, 
                          const std::wstring& targetTemplate,
                          const std::vector<EntityType>& entityTypes) -> void;

        /**
         * 从提取结果和翻译结果存储模板
         * @param extraction 提取结果（包含骨架和实体）
         * @param translatedText 翻译后的文本
         */
        auto StoreFromTranslation(const ExtractionResult& extraction,
                                  const std::wstring& translatedText) -> void;

        /**
         * 加载模板数据
         */
        auto Load() -> bool;

        /**
         * 保存模板数据
         */
        auto Save() -> bool;

        /**
         * 获取模板数量
         */
        [[nodiscard]] auto GetTemplateCount() const -> size_t;

        /**
         * 获取命中统计
         */
        [[nodiscard]] auto GetHitCount() const -> size_t { return m_hitCount; }
        [[nodiscard]] auto GetQueryCount() const -> size_t { return m_queryCount; }
        [[nodiscard]] auto GetHitRate() const -> float;

        /**
         * 重置统计
         */
        auto ResetStats() -> void;

        /**
         * 清空缓存
         */
        auto Clear() -> void;

        /**
         * 检查是否已初始化
         */
        [[nodiscard]] auto IsInitialized() const -> bool { return m_initialized; }

        /**
         * 设置数据变更标志（由VocabularyCache调用）
         */
        auto MarkDirty() -> void { m_dirty = true; }

        /**
         * 检查是否有未保存的更改
         */
        [[nodiscard]] auto IsDirty() const -> bool { return m_dirty; }

        /**
         * 清除脏标志
         */
        auto ClearDirty() -> void { m_dirty = false; }

        /**
         * 导出所有模板数据（用于持久化保存）
         * @return 所有模板条目的向量
         */
        [[nodiscard]] auto ExportAllTemplates() const -> std::vector<TemplateEntry>;

    private:
        TemplateCache() = default;
        ~TemplateCache() = default;
        TemplateCache(const TemplateCache&) = delete;
        TemplateCache& operator=(const TemplateCache&) = delete;

        std::unordered_map<std::wstring, TemplateEntry, WStringHash> m_templates;
        std::wstring m_filePath;
        mutable std::shared_mutex m_mutex;
        std::atomic<size_t> m_hitCount{0};
        std::atomic<size_t> m_queryCount{0};
        std::atomic<bool> m_initialized{false};
        std::atomic<bool> m_dirty{false};
    };
} // namespace RC::RealtimeTranslation
