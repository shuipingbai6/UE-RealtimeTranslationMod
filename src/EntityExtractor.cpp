#include <EntityExtractor.hpp>
#include <DebugLogger.hpp>
#include <algorithm>
#include <sstream>

namespace RC::RealtimeTranslation
{
    // 槽位标记常量
    static const std::wstring SLOT_NUM = L"{NUM}";
    static const std::wstring SLOT_PERCENT = L"{PERCENT}";
    static const std::wstring SLOT_DATE = L"{DATE}";

    auto EntityExtractor::GetPatterns() -> const std::vector<EntityPattern>&
    {
        static const std::vector<EntityPattern> patterns = {
            // 日期：支持年份（如 2026年3月27日）和不带年份（如 3月27日）
            // 注意：日期需要放在前面，优先匹配
            {
                EntityType::DATE,
                LR"((\d{4}年)?\d{1,2}月\d{1,2}日)",
                SLOT_DATE
            },
            // 百分比：支持空格（如 85 %, 99.9 %）
            {
                EntityType::PERCENT,
                LR"(\d+(\.\d+)?\s*%)",
                SLOT_PERCENT
            },
            // 数字：支持千位分隔符（如 1,000, 3.14）
            {
                EntityType::NUM,
                LR"([\d,]+\.?\d*)",
                SLOT_NUM
            }
        };
        return patterns;
    }

    auto EntityExtractor::GetSlotMarker(EntityType type) -> std::wstring
    {
        switch (type)
        {
            case EntityType::NUM:
                return SLOT_NUM;
            case EntityType::PERCENT:
                return SLOT_PERCENT;
            case EntityType::DATE:
                return SLOT_DATE;
            default:
                return L"";
        }
    }

    auto EntityExtractor::HasSlotMarkers(const std::wstring& text) -> bool
    {
        return text.find(SLOT_NUM) != std::wstring::npos ||
               text.find(SLOT_PERCENT) != std::wstring::npos ||
               text.find(SLOT_DATE) != std::wstring::npos;
    }

    auto EntityExtractor::EscapeRegex(const std::wstring& text) -> std::wstring
    {
        // 此函数用于转义正则表达式特殊字符（当前未使用，保留备用）
        static const std::wstring specialChars = LR"(\^$.|?*+()[]{})";
        std::wstring result;
        result.reserve(text.size() * 2);
        
        for (wchar_t c : text)
        {
            if (specialChars.find(c) != std::wstring::npos)
            {
                result += L'\\';
            }
            result += c;
        }
        return result;
    }

    auto EntityExtractor::ExtractEntities(const std::wstring& text) -> ExtractionResult
    {
        ExtractionResult result;
        result.SkeletonText = text;

        if (text.empty())
        {
            return result;
        }

        // 收集所有匹配的实体及其位置
        struct MatchInfo
        {
            size_t Start;
            size_t End;
            ExtractedEntity Entity;
        };
        std::vector<MatchInfo> allMatches;

        // 按顺序遍历所有模式
        for (const auto& pattern : GetPatterns())
        {
            try
            {
                std::wregex regex(pattern.Pattern);
                std::wsmatch match;
                
                std::wstring searchText = text;
                size_t offset = 0;
                
                // 查找所有匹配
                while (std::regex_search(searchText, match, regex))
                {
                    if (match.size() > 0)
                    {
                        MatchInfo info;
                        info.Start = offset + match.position(0);
                        info.End = info.Start + match.length(0);
                        
                        // 检查是否与已有匹配重叠
                        bool overlaps = false;
                        for (const auto& existing : allMatches)
                        {
                            if (!(info.End <= existing.Start || info.Start >= existing.End))
                            {
                                overlaps = true;
                                break;
                            }
                        }
                        
                        if (!overlaps)
                        {
                            info.Entity.Type = pattern.Type;
                            info.Entity.OriginalText = match.str(0);
                            info.Entity.SlotMarker = pattern.SlotMarker;
                            info.Entity.Position = info.Start;
                            info.Entity.Length = match.length(0);
                            
                            allMatches.push_back(info);
                        }
                    }
                    
                    // 继续搜索
                    offset += match.position(0) + match.length(0);
                    if (offset >= text.size()) break;
                    searchText = text.substr(offset);
                }
            }
            catch (const std::regex_error& e)
            {
                RT_DEBUG_LOG(std::wstring(L"[EntityExtractor] Regex error: ") + 
                    to_wstring(std::string(e.what())));
            }
        }

        // 按位置排序（从左到右）
        std::sort(allMatches.begin(), allMatches.end(), 
            [](const MatchInfo& a, const MatchInfo& b) {
                return a.Start < b.Start;
            });

        // 构建骨架文本
        if (!allMatches.empty())
        {
            std::wostringstream skeleton;
            size_t lastEnd = 0;
            
            for (const auto& match : allMatches)
            {
                // 添加匹配前的文本
                if (match.Start > lastEnd)
                {
                    skeleton << text.substr(lastEnd, match.Start - lastEnd);
                }
                
                // 添加槽位标记
                skeleton << match.Entity.SlotMarker;
                lastEnd = match.End;
                
                // 添加到实体列表
                result.Entities.push_back(match.Entity);
            }
            
            // 添加最后的文本
            if (lastEnd < text.size())
            {
                skeleton << text.substr(lastEnd);
            }
            
            result.SkeletonText = skeleton.str();
        }

        RT_DEBUG_LOG(std::wstring(L"[EntityExtractor] Extracted ") + 
            std::to_wstring(result.Entities.size()) + L" entities from: '" + text + L"'");
        RT_DEBUG_LOG(std::wstring(L"[EntityExtractor] Skeleton: '") + result.SkeletonText + L"'");

        return result;
    }

    auto EntityExtractor::FillSlots(const std::wstring& templateText, 
                                     const std::vector<ExtractedEntity>& entities) -> std::wstring
    {
        if (entities.empty())
        {
            return templateText;
        }

        std::wstring result = templateText;
        
        // 按槽位类型分组并记录索引
        struct SlotInfo
        {
            std::wstring Marker;
            size_t OriginalIndex;  // 在原始实体列表中的索引
        };
        
        // 查找所有槽位标记并记录位置
        std::vector<std::pair<size_t, SlotInfo>> slots;
        
        // 查找所有槽位
        size_t pos = 0;
        while (pos < result.size())
        {
            bool found = false;
            
            // 检查是否是某个槽位标记的开始
            for (size_t i = 0; i < entities.size(); ++i)
            {
                const auto& marker = entities[i].SlotMarker;
                if (result.compare(pos, marker.size(), marker) == 0)
                {
                    slots.push_back({pos, {marker, i}});
                    pos += marker.size();
                    found = true;
                    break;
                }
            }
            
            if (!found)
            {
                pos++;
            }
        }

        // 从后向前替换（避免位置偏移问题）
        // 但需要按照实体顺序对应槽位顺序
        std::sort(slots.begin(), slots.end(), 
            [](const auto& a, const auto& b) { return a.first > b.first; });

        // 创建槽位索引映射：第n个出现的槽位对应第n个实体
        std::vector<size_t> slotOrder;
        for (const auto& [position, info] : slots)
        {
            slotOrder.push_back(info.OriginalIndex);
        }
        // 反转以获得从前到后的顺序
        std::reverse(slotOrder.begin(), slotOrder.end());

        // 按实体在原文中出现的顺序，对应模板中槽位出现的顺序
        // 重新映射：模板中第i个槽位应该填充第i个实体
        size_t entityIndex = 0;
        for (auto it = slots.rbegin(); it != slots.rend(); ++it, ++entityIndex)
        {
            if (entityIndex < entities.size())
            {
                size_t startPos = it->first;
                const auto& marker = it->second.Marker;
                
                result.replace(startPos, marker.size(), entities[entityIndex].OriginalText);
            }
        }

        RT_DEBUG_LOG(std::wstring(L"[EntityExtractor] Filled slots: '") + templateText + 
            L"' -> '" + result + L"'");

        return result;
    }
} // namespace RC::RealtimeTranslation
