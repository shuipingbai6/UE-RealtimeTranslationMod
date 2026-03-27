# RealtimeTranslationMod

A UE4/UE5 game real-time translation mod based on UE4SS, utilizing AI APIs for instant in-game text translation.（It's currently in the preliminary stage with only a general framework, but the core functions are usable.）

## Features

- **Real-time Translation**: Automatically captures in-game text and calls AI APIs for translation
- **Vocabulary Cache**: Translated text is automatically cached to avoid duplicate requests
- **Multi-threaded Processing**: Asynchronous translation queue that doesn't block the game's main thread
- **Win32 Control Panel**: Standalone window to control translation on/off and configure API settings
- **ImGui Integration Panel**: In-game UE4SS panel displaying translation status
- **Shared Memory Communication**: Supports external programs reading translation status

## Requirements

- [UE4SS](https://github.com/UE4SS-RE/RE-UE4SS) 2.6+
- Visual Studio 2022 (MSVC)
- CMake 3.22+
- C++23 support

## Building

### Prerequisites

1. Clone the UE4SS main project

```bash
git clone https://github.com/UE4SS-RE/RE-UE4SS.git
cd RE-UE4SS
```

2. Place this mod in the `cppmods/` directory

```
RE-UE4SS/
└── cppmods/
    └── RealtimeTranslationMod/
        ├── CMakeLists.txt
        ├── include/
        ├── src/
        └── config/
```

### Build Steps

```powershell
cd RE-UE4SS
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The compiled output is located at `build/cppmods/RealtimeTranslationMod/Release/RealtimeTranslationMod.dll`

## Installation

1. Build to generate `RealtimeTranslationMod.dll`
2. Copy the DLL to the game's UE4SS Mods directory:

```
<GameDirectory>/Binaries/Win64/Mods/RealtimeTranslationMod/
├── RealtimeTranslationMod.dll
├── translation_config.json
└── vocabulary.json
```

3. Enable the mod in `mods.txt`:

```
RealtimeTranslationMod : 1
```

## Configuration

### translation_config.json

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

### Configuration Reference

| Field | Description |
|-------|-------------|
| `api_endpoint` | AI API endpoint URL |
| `api_key` | API key |
| `model` | Model name to use |
| `source_language` | Source language, `auto` for auto-detection |
| `target_language` | Target language |
| `min_text_length` | Minimum text length for translation |

## Usage

1. Launch the game, UE4SS will automatically load the mod
2. Press the UE4SS console hotkey (default `~`) to open the panel
3. Switch to the "RealtimeTranslationMod" tab
4. Configure the API endpoint and key
5. Click "Start" to begin translation

### Win32 Control Panel

The mod creates a standalone Win32 window on startup, allowing control outside the game:

- Start/Stop translation
- Modify API configuration
- Save vocabulary
- Reload configuration
- Refresh game widgets

## API Compatibility

Supports OpenAI-compatible API formats, including:

- OpenAI GPT series
- Azure OpenAI
- Locally deployed LLMs (e.g., Ollama, vLLM)
- Other compatible APIs

### Custom API Endpoint Example

```json
{
  "api_endpoint": "http://localhost:11434/v1/chat/completions",
  "model": "qwen2.5:7b"
}
```

## How It Works

```
Game Text Display
     ↓
TextHookManager intercepts FText/FString
     ↓
VocabularyCache lookup
     ↓ (miss)
TranslationManager enqueue
     ↓
AIProvider calls API
     ↓
TextApplicator applies translation
     ↓
VocabularyCache stores
```

## File Structure

```
RealtimeTranslationMod/
├── include/                    # Header files
│   ├── AIProvider.hpp          # AI API wrapper
│   ├── ConfigManager.hpp       # Configuration management
│   ├── NetworkClient.hpp       # HTTP client
│   ├── TextHookManager.hpp     # Text hook management
│   ├── TextApplicator.hpp      # Translation applicator
│   ├── TranslationManager.hpp  # Translation queue management
│   ├── VocabularyCache.hpp     # Vocabulary cache
│   ├── UIPropertyScanner.hpp   # UI property scanner
│   └── UI/                     # UI components
│       ├── TranslationUI.hpp   # ImGui panel
│       └── Win32TranslationUI.hpp  # Win32 window
├── src/                        # Source files
├── config/                     # Configuration templates
└── CMakeLists.txt              # Build configuration
```

## Known Issues

- Some games may require hook strategy adjustments
- Delays may occur when large amounts of text appear simultaneously
- Some dynamically generated text may not be captured
- Custom functions cannot be translated (solution in progress, expected to resolve 95% automatically)

## License

GPL-3.0 License
（adopt GPL 3.0 in the hope that all improvements will be contributed back to the community, so that the basic framework will become increasingly robust.）

## Acknowledgments

- [UE4SS](https://github.com/UE4SS-RE/RE-UE4SS) - Unreal Engine 4/5 Scripting System
- [concurrentqueue](https://github.com/cameron314/concurrentqueue) - High-performance concurrent queue
