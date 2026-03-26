#pragma once

#include "TranslationCommon.hpp"
#include <File/File.hpp>
#include <string>
#include <filesystem>

namespace RC::RealtimeTranslation
{
    /**
     * 配置管理器
     * 负责加载、保存和验证Mod配置
     */
    class ConfigManager
    {
    public:
        static ConfigManager& Instance();

        // 初始化配置管理器
        auto Initialize(const std::wstring& configPath) -> bool;

        // 加载配置文件
        auto LoadConfig() -> bool;

        // 保存配置文件
        auto SaveConfig() -> bool;

        // 获取当前配置
        [[nodiscard]] auto GetConfig() const -> const ModConfig& { return m_config; }
        auto GetConfigMutable() -> ModConfig& { return m_config; }

        // 更新配置
        auto UpdateConfig(const ModConfig& newConfig) -> void;

        // 验证配置
        [[nodiscard]] auto ValidateConfig() const -> bool;

        // 获取配置文件路径
        [[nodiscard]] auto GetConfigPath() const -> const std::wstring& { return m_configPath; }

        // 创建默认配置文件
        auto CreateDefaultConfig() -> bool;

        // 共享内存支持
        auto SyncToSharedMemory() -> void;
        auto SyncFromSharedMemory() -> bool;
        auto CheckSharedMemoryRequests() -> void;

    private:
        ConfigManager() = default;
        ~ConfigManager() = default;
        ConfigManager(const ConfigManager&) = delete;
        ConfigManager& operator=(const ConfigManager&) = delete;

        auto ParseConfigFromJson(const std::wstring& jsonContent) -> void;
        auto CreateDefaultConfigInternal() -> bool;
        auto SaveConfigInternal() -> bool;

        ModConfig m_config;
        std::wstring m_configPath;
        mutable std::shared_mutex m_mutex;
        bool m_initialized{false};
    };
} // namespace RC::RealtimeTranslation
