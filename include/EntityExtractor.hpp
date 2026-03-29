#pragma once

#include "TranslationCommon.hpp"
#include <vector>
#include <string>
#include <optional>
#include <regex>

namespace RC::RealtimeTranslation
{
    /**
     * 实体类型枚举
     * 定义可以从文本中提取的实体类型
     */
    enum class EntityType
    {
        NUM,        // 数字（支持千位分隔符）
        PERCENT,    // 百分比（支持空格）
        DATE        // 日期（支持年份）
    };

    /**
     * 提取的实体结构
     */
    struct ExtractedEntity
    {
        EntityType Type;            // 实体类型
        std::wstring OriginalText;  // 原始文本
        std::wstring SlotMarker;    // 槽位标记（如 {NUM}, {PERCENT}, {DATE}）
        size_t Position;            // 在原文中的位置
        size_t Length;              // 原文长度
    };

    /**
     * 实体提取结果
     */
    struct ExtractionResult
    {
        std::wstring SkeletonText;              // 骨架文本（实体替换为槽位标记）
        std::vector<ExtractedEntity> Entities;  // 提取的实体列表（按从左到右顺序）
    };

    /**
     * 实体提取器
     * 从文本中提取数字、百分比、日期等实体，生成骨架文本
     */
    class EntityExtractor
    {
    public:
        /**
         * 从文本中提取实体
         * @param text 原始文本
         * @return 提取结果，包含骨架文本和实体列表
         */
        static auto ExtractEntities(const std::wstring& text) -> ExtractionResult;

        /**
         * 将实体填充到模板槽位
         * @param templateText 模板文本（包含槽位标记）
         * @param entities 实体列表
         * @return 填充后的最终文本
         */
        static auto FillSlots(const std::wstring& templateText, const std::vector<ExtractedEntity>& entities) -> std::wstring;

        /**
         * 获取实体类型对应的槽位标记
         * @param type 实体类型
         * @return 槽位标记字符串
         */
        static auto GetSlotMarker(EntityType type) -> std::wstring;

        /**
         * 检查文本是否包含槽位标记
         * @param text 文本
         * @return 是否包含槽位标记
         */
        static auto HasSlotMarkers(const std::wstring& text) -> bool;

    private:
        // 实体匹配模式定义
        struct EntityPattern
        {
            EntityType Type;
            std::wstring Pattern;       // 正则表达式模式
            std::wstring SlotMarker;    // 槽位标记
        };

        // 获取所有实体匹配模式
        static auto GetPatterns() -> const std::vector<EntityPattern>&;

        // 转义正则表达式特殊字符
        static auto EscapeRegex(const std::wstring& text) -> std::wstring;
    };
} // namespace RC::RealtimeTranslation
