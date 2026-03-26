#include <VocabularyCache.hpp>
#include <DynamicOutput/DynamicOutput.hpp>
#include <DebugLogger.hpp>

#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <mutex>
#include <codecvt>
#include <locale>

namespace RC::RealtimeTranslation
{
    // JSON辅助函数（与ConfigManager共用，这里简化实现）
    static auto JsonEscape(const std::wstring& s) -> std::wstring
    {
        std::wstring result;
        result.reserve(s.size());
        for (auto c : s)
        {
            switch (c)
            {
                case L'"': result += L"\\\""; break;
                case L'\\': result += L"\\\\"; break;
                case L'\n': result += L"\\n"; break;
                case L'\r': result += L"\\r"; break;
                case L'\t': result += L"\\t"; break;
                default: result += c; break;
            }
        }
        return result;
    }

    static auto JsonUnescape(const std::wstring& s) -> std::wstring
    {
        std::wstring result;
        result.reserve(s.size());
        bool escape = false;
        for (auto c : s)
        {
            if (escape)
            {
                switch (c)
                {
                    case L'"': result += L'"'; break;
                    case L'\\': result += L'\\'; break;
                    case L'n': result += L'\n'; break;
                    case L'r': result += L'\r'; break;
                    case L't': result += L'\t'; break;
                    default: result += c; break;
                }
                escape = false;
            }
            else if (c == L'\\')
            {
                escape = true;
            }
            else
            {
                result += c;
            }
        }
        return result;
    }

    VocabularyCache& VocabularyCache::Instance()
    {
        static VocabularyCache instance;
        return instance;
    }

    VocabularyCache::~VocabularyCache()
    {
        m_stopSaveThread = true;
        m_saveCv.notify_all();
        if (m_saveThread.joinable())
        {
            m_saveThread.join();
        }

        // 最后保存一次
        if (m_dirty)
        {
            Save();
        }
    }

    auto VocabularyCache::Initialize(const std::wstring& filePath) -> bool
    {
        std::unique_lock lock(m_mutex);
        m_filePath = filePath;

        // 启动后台保存线程
        if (m_autoSave && !m_saveThread.joinable())
        {
            m_stopSaveThread = false;
            m_saveThread = std::thread(&VocabularyCache::BackgroundSaveThread, this);
        }

        lock.unlock();
        m_initialized = true;

        // 加载词库
        return Load();
    }

    auto VocabularyCache::Get(const std::wstring& original) -> std::optional<std::wstring>
    {
        m_queryCount++;

        std::shared_lock lock(m_mutex);
        auto it = m_cache.find(original);
        if (it != m_cache.end())
        {
            m_hitCount++;
            return it->second;
        }

        return std::nullopt;
    }

    auto VocabularyCache::Lookup(const std::wstring& original, std::wstring& outTranslated) -> bool
    {
        m_queryCount++;

        std::shared_lock lock(m_mutex);
        auto it = m_cache.find(original);
        if (it != m_cache.end())
        {
            m_hitCount++;
            outTranslated = it->second;
            return true;
        }

        return false;
    }

    auto VocabularyCache::Store(const std::wstring& original, const std::wstring& translated) -> void
    {
        {
            std::unique_lock lock(m_mutex);
            m_cache[original] = translated;
        }
        m_dirty = true;

        RT_DEBUG_LOG(std::wstring(L"[VocabularyCache] Stored: '") + original + L"' -> '" + translated + L"'");

        Save();
    }

    auto VocabularyCache::StoreBatch(const std::vector<std::pair<std::wstring, std::wstring>>& entries) -> void
    {
        {
            std::unique_lock lock(m_mutex);
            for (const auto& [original, translated] : entries)
            {
                m_cache[original] = translated;
            }
        }
        m_dirty = true;

        if (!m_autoSave)
        {
            Save();
        }
    }

    auto VocabularyCache::Load() -> bool
    {
        RT_DEBUG_LOG(std::wstring(L"[VocabularyCache] Loading from: ") + m_filePath);

        if (!std::filesystem::exists(m_filePath))
        {
            RT_DEBUG_LOG(L"[VocabularyCache] File not found, will create new one");
            Output::send<LogLevel::Normal>(STR("[VocabularyCache] Vocabulary file not found, will create new one: {}\n"), m_filePath);
            return true;
        }

        std::wifstream file(m_filePath);
        if (!file.is_open())
        {
            RT_DEBUG_LOG(L"[VocabularyCache] Failed to open file");
            Output::send<LogLevel::Error>(STR("[VocabularyCache] Failed to open vocabulary file: {}\n"), m_filePath);
            return false;
        }

        file.imbue(std::locale(file.getloc(), new std::codecvt_utf8<wchar_t>));

        std::wstringstream buffer;
        buffer << file.rdbuf();
        std::wstring content = buffer.str();
        file.close();

        RT_DEBUG_LOG(std::wstring(L"[VocabularyCache] File content length: ") + std::to_wstring(content.size()));

        std::unique_lock lock(m_mutex);
        m_cache.clear();

        size_t entriesStart = content.find(L"\"entries\"");
        if (entriesStart == std::wstring::npos)
        {
            RT_DEBUG_LOG(L"[VocabularyCache] No entries found in vocabulary file");
            Output::send<LogLevel::Warning>(STR("[VocabularyCache] No entries found in vocabulary file.\n"));
            return true;
        }

        entriesStart = content.find(L'{', entriesStart);
        if (entriesStart == std::wstring::npos) return true;

        size_t pos = entriesStart + 1;
        int braceCount = 1;
        int entryCount = 0;

        while (pos < content.size() && braceCount > 0)
        {
            while (pos < content.size() && (content[pos] == L' ' || content[pos] == L'\t' ||
                   content[pos] == L'\n' || content[pos] == L'\r' || content[pos] == L','))
                pos++;

            if (pos >= content.size()) break;

            if (content[pos] == L'}')
            {
                braceCount--;
                pos++;
                continue;
            }

            if (content[pos] != L'"') break;
            pos++;
            size_t keyStart = pos;
            bool escape = false;
            while (pos < content.size())
            {
                if (escape) escape = false;
                else if (content[pos] == L'\\') escape = true;
                else if (content[pos] == L'"') break;
                pos++;
            }
            std::wstring key = JsonUnescape(content.substr(keyStart, pos - keyStart));
            pos++;

            while (pos < content.size() && content[pos] != L':') pos++;
            pos++;
            while (pos < content.size() && (content[pos] == L' ' || content[pos] == L'\t'))
                pos++;

            if (pos >= content.size() || content[pos] != L'"') break;
            pos++;
            size_t valueStart = pos;
            escape = false;
            while (pos < content.size())
            {
                if (escape) escape = false;
                else if (content[pos] == L'\\') escape = true;
                else if (content[pos] == L'"') break;
                pos++;
            }
            std::wstring value = JsonUnescape(content.substr(valueStart, pos - valueStart));
            pos++;

            if (!key.empty())
            {
                m_cache[key] = value;
                entryCount++;
            }
        }

        RT_DEBUG_LOG(std::wstring(L"[VocabularyCache] Loaded ") + std::to_wstring(entryCount) + L" entries");
        Output::send<LogLevel::Normal>(STR("[VocabularyCache] Loaded {} entries from: {}\n"), m_cache.size(), m_filePath);
        return true;
    }

    auto VocabularyCache::Save() -> bool
    {
        std::wstring filePathCopy;
        std::unordered_map<std::wstring, std::wstring, WStringHash> cacheCopy;
        {
            std::shared_lock lock(m_mutex);
            filePathCopy = m_filePath;
            cacheCopy = m_cache;
        }

        RT_DEBUG_LOG(std::wstring(L"[VocabularyCache] Saving to: ") + filePathCopy);
        RT_DEBUG_LOG(std::wstring(L"[VocabularyCache] Entry count: ") + std::to_wstring(cacheCopy.size()));

        try
        {
            std::filesystem::path filePath(filePathCopy);
            auto parentDir = filePath.parent_path();
            if (!parentDir.empty() && !std::filesystem::exists(parentDir))
            {
                std::filesystem::create_directories(parentDir);
            }

            std::wstringstream json;
            json << L"{\n";
            json << L"  \"version\": \"1.0\",\n";
            json << L"  \"created\": \"" << std::chrono::system_clock::now().time_since_epoch().count() << L"\",\n";
            json << L"  \"entries\": {\n";

            bool first = true;
            for (const auto& [original, translated] : cacheCopy)
            {
                if (!first) json << L",\n";
                first = false;
                json << L"    \"" << JsonEscape(original) << L"\": \"" << JsonEscape(translated) << L"\"";
            }

            json << L"\n  }\n";
            json << L"}\n";

            std::wofstream file(filePath);
            if (!file.is_open())
            {
                RT_DEBUG_LOG(std::wstring(L"[VocabularyCache] Failed to open file for writing: ") + filePathCopy);
                Output::send<LogLevel::Error>(STR("[VocabularyCache] Failed to open vocabulary file for writing: {}\n"), filePathCopy);
                return false;
            }

            file.imbue(std::locale(file.getloc(), new std::codecvt_utf8<wchar_t>));

            file << json.str();
            file.flush();
            file.close();

            m_dirty = false;
            RT_DEBUG_LOG(std::wstring(L"[VocabularyCache] Saved successfully: ") + std::to_wstring(cacheCopy.size()) + L" entries");
            Output::send<LogLevel::Verbose>(STR("[VocabularyCache] Saved {} entries to: {}\n"), cacheCopy.size(), filePathCopy);
            return true;
        }
        catch (const std::exception& e)
        {
            RT_DEBUG_LOG(std::wstring(L"[VocabularyCache] Exception: ") + to_wstring(std::string(e.what())));
            Output::send<LogLevel::Error>(STR("[VocabularyCache] Exception while saving: {}\n"),
                to_wstring(std::string(e.what())));
            return false;
        }
    }

    auto VocabularyCache::GetEntryCount() const -> size_t
    {
        std::shared_lock lock(m_mutex);
        return m_cache.size();
    }

    auto VocabularyCache::GetHitRate() const -> float
    {
        size_t total = m_queryCount;
        return total > 0 ? static_cast<float>(m_hitCount) / static_cast<float>(total) : 0.0f;
    }

    auto VocabularyCache::ResetStats() -> void
    {
        m_hitCount = 0;
        m_queryCount = 0;
    }

    auto VocabularyCache::Clear() -> void
    {
        std::unique_lock lock(m_mutex);
        m_cache.clear();
        m_dirty = true;
    }

    auto VocabularyCache::SetAutoSave(bool enabled, int intervalMs) -> void
    {
        m_autoSave = enabled;
        m_saveIntervalMs = intervalMs;
    }

    auto VocabularyCache::FlushIfDirty() -> void
    {
        if (m_dirty)
        {
            Save();
        }
    }

    auto VocabularyCache::BackgroundSaveThread() -> void
    {
        while (!m_stopSaveThread)
        {
            std::unique_lock<std::mutex> lock(m_saveMutex);
            m_saveCv.wait_for(lock, std::chrono::milliseconds(m_saveIntervalMs));

            if (m_dirty && m_autoSave)
            {
                Save();
            }
        }
    }
} // namespace RC::RealtimeTranslation
