#pragma once

#include "TranslationCommon.hpp"
#include "TemplateCache.hpp"
#include <shared_mutex>
#include <unordered_map>
#include <optional>
#include <chrono>

namespace RC::RealtimeTranslation
{
    /**
     * 永久词库缓存
     * 实现翻译结果的持久化存储，支持线程安全访问
     * 集成模板缓存功能
     */
    class VocabularyCache
    {
    public:
        static VocabularyCache& Instance();

        // 初始化词库
        auto Initialize(const std::wstring& filePath) -> bool;

        // 查询词库（完整文本精确匹配）
        [[nodiscard]] auto Get(const std::wstring& original) -> std::optional<std::wstring>;

        // 查询词库（输出参数版本，用于性能优化）
        [[nodiscard]] auto Lookup(const std::wstring& original, std::wstring& outTranslated) -> bool;

        // 查询模板缓存
        [[nodiscard]] auto GetTemplate(const std::wstring& skeleton) -> std::optional<TemplateEntry>;

        // 查询模板缓存（输出参数版本）
        [[nodiscard]] auto LookupTemplate(const std::wstring& skeleton, TemplateEntry& outEntry) -> bool;

        // 存储翻译结果（同时写入内存和文件）
        auto Store(const std::wstring& original, const std::wstring& translated) -> void;

        // 存储翻译模板
        auto StoreTemplate(const std::wstring& sourceSkeleton,
                          const std::wstring& targetTemplate,
                          const std::vector<EntityType>& entityTypes) -> void;

        // 从提取结果和翻译结果存储模板
        auto StoreTemplateFromTranslation(const ExtractionResult& extraction,
                                          const std::wstring& translatedText) -> void;

        // 批量存储
        auto StoreBatch(const std::vector<std::pair<std::wstring, std::wstring>>& entries) -> void;

        // 加载词库文件
        auto Load() -> bool;

        // 保存词库文件
        auto Save() -> bool;

        // 获取统计信息
        [[nodiscard]] auto GetEntryCount() const -> size_t;
        [[nodiscard]] auto GetTemplateCount() const -> size_t;
        [[nodiscard]] auto GetHitCount() const -> size_t { return m_hitCount; }
        [[nodiscard]] auto GetQueryCount() const -> size_t { return m_queryCount; }
        [[nodiscard]] auto GetHitRate() const -> float;

        // 模板缓存统计
        [[nodiscard]] auto GetTemplateHitCount() const -> size_t;
        [[nodiscard]] auto GetTemplateQueryCount() const -> size_t;
        [[nodiscard]] auto GetTemplateHitRate() const -> float;

        // 重置统计
        auto ResetStats() -> void;

        // 清空词库
        auto Clear() -> void;

        // 检查是否已初始化
        [[nodiscard]] auto IsInitialized() const -> bool { return m_initialized; }

        // 获取词库文件路径
        [[nodiscard]] auto GetFilePath() const -> const std::wstring& { return m_filePath; }

        // 设置自动保存
        auto SetAutoSave(bool enabled, int intervalMs = 5000) -> void;

        // 手动触发保存（如果脏）
        auto FlushIfDirty() -> void;

    private:
        VocabularyCache() = default;
        ~VocabularyCache();
        VocabularyCache(const VocabularyCache&) = delete;
        VocabularyCache& operator=(const VocabularyCache&) = delete;

        // 后台保存线程
        auto BackgroundSaveThread() -> void;

        std::unordered_map<std::wstring, std::wstring, WStringHash> m_cache;
        std::wstring m_filePath;
        mutable std::shared_mutex m_mutex;
        std::atomic<size_t> m_hitCount{0};
        std::atomic<size_t> m_queryCount{0};
        std::atomic<bool> m_initialized{false};
        std::atomic<bool> m_dirty{false};
        std::atomic<bool> m_autoSave{true};
        int m_saveIntervalMs{5000};

        // 后台保存线程
        std::thread m_saveThread;
        std::atomic<bool> m_stopSaveThread{false};
        std::condition_variable m_saveCv;
        std::mutex m_saveMutex;
    };
} // namespace RC::RealtimeTranslation
