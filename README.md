# RealtimeTranslationMod

基于 UE4SS 的 UE4/UE5 游戏实时翻译 Mod，利用 AI API 实现游戏内文本的即时翻译。

## 功能特性

- **实时翻译**：自动捕获游戏中的文本并调用 AI API 进行翻译
- **词汇缓存**：已翻译的文本自动缓存，避免重复请求
- **多线程处理**：异步翻译队列，不阻塞游戏主线程
- **Win32 控制面板**：独立窗口控制翻译开关、配置 API
- **ImGui 集成面板**：游戏内 UE4SS 面板显示翻译状态
- **共享内存通信**：支持外部程序读取翻译状态

## 环境要求

- [UE4SS](https://github.com/UE4SS-RE/RE-UE4SS) 2.6+
- Visual Studio 2022 (MSVC)
- CMake 3.22+
- C++23 支持

## 编译

### 前置条件

1. 克隆 UE4SS 主项目

```bash
git clone https://github.com/UE4SS-RE/RE-UE4SS.git
cd RE-UE4SS
```

1. 将本 Mod 放入 `cppmods/` 目录

```
RE-UE4SS/
└── cppmods/
    └── RealtimeTranslationMod/
        ├── CMakeLists.txt
        ├── include/
        ├── src/
        └── config/
```

### 编译步骤

```powershell
cd RE-UE4SS
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

编译产物位于 `build/cppmods/RealtimeTranslationMod/Release/RealtimeTranslationMod.dll`

## 安装

1. 编译生成 `RealtimeTranslationMod.dll`
2. 将 DLL 复制到游戏的 UE4SS Mods 目录：

```
<游戏目录>/Binaries/Win64/Mods/RealtimeTranslationMod/
├── RealtimeTranslationMod.dll
├── translation_config.json
└── vocabulary.json
```

1. 在 `mods.txt` 中启用 Mod：

```
RealtimeTranslationMod : 1
```

## 配置

### translation\_config.json

```json
{
  "ai_provider": {
    "api_endpoint": "https://api.openai.com/v1/chat/completions",
    "api_key": "your-api-key-here",
    "model": "gpt-4",
    "timeout_ms": 10000,
    "max_retries": 3,
    "request_interval_ms": 500,
    "max_requests_per_second": 2
  },
  "translation": {
    "source_language": "auto",
    "target_language": "zh-CN"
  },
  "hooks": {
    "min_text_length": 2
  },
  "vocabulary": {
    "file_path": "vocabulary.json",
    "auto_save": true,
    "save_interval_ms": 5000
  }
}
```

### 配置说明

| 字段                | 说明               |
| ----------------- | ---------------- |
| `api_endpoint`    | AI API 端点地址      |
| `api_key`         | API 密钥           |
| `model`           | 使用的模型名称          |
| `source_language` | 源语言，`auto` 为自动检测 |
| `target_language` | 目标语言             |
| `min_text_length` | 最小翻译文本长度         |

## 使用方法

1. 启动游戏，UE4SS 自动加载 Mod
2. 按 UE4SS 控制台快捷键（默认 `~`）打开面板
3. 切换到 "RealtimeTranslationMod" 标签页
4. 配置 API 端点和密钥
5. 点击 "Start" 开始翻译

### Win32 控制面板

Mod 启动后会创建独立的 Win32 窗口，可在游戏外控制：

- 启动/停止翻译
- 修改 API 配置
- 保存词汇表
- 重新加载配置
- 重新刷新游戏

## API 兼容性

支持 OpenAI 兼容的 API 格式，包括：

- OpenAI GPT 系列
- Azure OpenAI
- 本地部署的 LLM（如 Ollama、vLLM）
- 其他兼容 API

### 自定义 API 端点示例

```json
{
  "api_endpoint": "http://localhost:11434/v1/chat/completions",
  "model": "qwen2.5:7b"
}
```

## 工作原理

```
游戏文本显示
     ↓
TextHookManager 拦截 FText/FString
     ↓
VocabularyCache 查询缓存
     ↓ (未命中)
TranslationManager 入队
     ↓
AIProvider 调用 API
     ↓
TextApplicator 应用翻译
     ↓
VocabularyCache 存储
```

## 文件结构

```
RealtimeTranslationMod/
├── include/                    # 头文件
│   ├── AIProvider.hpp          # AI API 封装
│   ├── ConfigManager.hpp       # 配置管理
│   ├── NetworkClient.hpp       # HTTP 客户端
│   ├── TextHookManager.hpp     # 文本 Hook 管理
│   ├── TextApplicator.hpp      # 翻译应用器
│   ├── TranslationManager.hpp  # 翻译队列管理
│   ├── VocabularyCache.hpp     # 词汇缓存
│   ├── UIPropertyScanner.hpp   # UI 属性扫描
│   └── UI/                     # UI 组件
│       ├── TranslationUI.hpp   # ImGui 面板
│       └── Win32TranslationUI.hpp  # Win32 窗口
├── src/                        # 源文件
├── config/                     # 配置模板
└── CMakeLists.txt              # 构建配置
```

## 已知问题

- 部分游戏可能需要调整 Hook 策略
- 大量文本同时出现时可能有延迟
- 某些动态生成的文本可能无法捕获
- 自定义函数无法翻译（已经想好思路，预计能够自动解决95%）

## 许可证

GPL-3.0 License

## 致谢

- [UE4SS](https://github.com/UE4SS-RE/RE-UE4SS) - Unreal Engine 4/5 Scripting System
- [concurrentqueue](https://github.com/cameron314/concurrentqueue) - 高性能并发队列

