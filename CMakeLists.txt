cmake_minimum_required(VERSION 3.22)
set(TARGET RealtimeTranslationMod)
project(${TARGET})

include(FetchContent)

# 并发队列（异步翻译用）- header-only库
FetchContent_Declare(
    concurrentqueue
    GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
    GIT_TAG c68072129c8a5b4025122ca5a0c82ab14b30cb03
)
FetchContent_MakeAvailable(concurrentqueue)

# 源文件
set(${TARGET}_Sources
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dllmain.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/TranslationMod.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/TranslationManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/AIProvider.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ConfigManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/VocabularyCache.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/TemplateCache.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EntityExtractor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/NetworkClient.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/TextHookManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/HookTargetSelector.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/TextApplicator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/TranslationUI.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/Win32TranslationUI.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UIPropertyScanner.cpp"
)

# 创建共享库
add_library(${TARGET} SHARED ${${TARGET}_Sources})

# 包含目录
target_include_directories(${TARGET} PRIVATE "include")

# 链接UE4SS核心（包含ImGui）
target_link_libraries(${TARGET} PUBLIC UE4SS)

# 链接并发队列（header-only）
target_link_libraries(${TARGET} PRIVATE concurrentqueue)

# Windows系统库（网络）
target_link_libraries(${TARGET} PRIVATE
    winhttp      # HTTP请求（AI API调用）
    user32       # Win32 UI
    comctl32     # Win32 Common Controls
)

# C++23标准
target_compile_features(${TARGET} PUBLIC cxx_std_23)

# UTF-8编码支持
target_compile_options(${TARGET} PRIVATE
    /utf-8       # 源文件和执行字符集都使用UTF-8
    /EHsc        # 异常处理
)

# 运行时库设置（与UE4SS一致）
set_property(TARGET ${TARGET} PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
