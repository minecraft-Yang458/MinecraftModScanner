cmake_minimum_required(VERSION 3.21)

project(MinecraftModScanner VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /utf-8 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network)
qt_standard_project_setup(REQUIRES 6.5)

set(APP_CORE_SOURCES
    src/AppConfig.cpp
    src/AppConfig.h
    src/AppLog.cpp
    src/AppLog.h
    src/SearchTypes.cpp
    src/SearchTypes.h
    src/LicenseUtils.cpp
    src/LicenseUtils.h
    src/ScanCleaner.cpp
    src/ScanCleaner.h
    src/HttpClient.cpp
    src/HttpClient.h
    src/BasePlatformClient.cpp
    src/BasePlatformClient.h
    src/ModrinthClient.cpp
    src/ModrinthClient.h
    src/CurseForgeClient.cpp
    src/CurseForgeClient.h
    src/NetworkTaskManager.cpp
    src/NetworkTaskManager.h
    src/McModWikiClient.cpp
    src/McModWikiClient.h
    src/ResultModel.cpp
    src/ResultModel.h
    src/ActionButtonDelegate.cpp
    src/ActionButtonDelegate.h
    src/ExportService.cpp
    src/ExportService.h
    src/DetailsDialog.cpp
    src/DetailsDialog.h
    src/UrlLauncher.cpp
    src/UrlLauncher.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/ManualSearchDialog.cpp
    src/ManualSearchDialog.h
    src/SummaryDialog.cpp
    src/SummaryDialog.h
    src/MainWindow.cpp
    src/MainWindow.h
)

set(APP_SOURCES
    src/main.cpp
    ${APP_CORE_SOURCES}
)

qt_add_executable(MinecraftModScanner WIN32 ${APP_SOURCES})
qt_add_resources(MinecraftModScanner "app_resources"
    PREFIX "/"
    BASE "resources"
    FILES
        resources/style.qss
        resources/app.svg
)

target_link_libraries(MinecraftModScanner PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

if(WIN32)
    target_link_libraries(MinecraftModScanner PRIVATE shell32)
endif()

target_compile_definitions(MinecraftModScanner PRIVATE
    MMS_VERSION="${PROJECT_VERSION}"
)

set_target_properties(MinecraftModScanner PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Minecraft Mod Scanner"
)

option(MMS_DEPLOY_QT_RUNTIME
    "Run windeployqt after build so the EXE can be launched directly" ON)
if(WIN32 AND MMS_DEPLOY_QT_RUNTIME)
    if(TARGET Qt6::windeployqt)
        add_custom_command(TARGET MinecraftModScanner POST_BUILD
            COMMAND Qt6::windeployqt
                --no-translations
                --compiler-runtime
                "$<IF:$<CONFIG:Debug>,--debug,--release>"
                "$<TARGET_FILE:MinecraftModScanner>"
            COMMENT "Deploying Qt runtime next to MinecraftModScanner.exe"
            VERBATIM
        )
    else()
        message(WARNING
            "Qt6::windeployqt is unavailable; the built EXE may need Qt DLLs on PATH. "
            "Use windeployqt manually or run build.ps1 -Deploy.")
    endif()
endif()

install(TARGETS MinecraftModScanner
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)
install(FILES README.md LICENSE NOTICE DESTINATION .)

if(WIN32)
    qt_generate_deploy_app_script(
        TARGET MinecraftModScanner
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.5 QUIET COMPONENTS Test)
    if(TARGET Qt6::Test)
        qt_add_executable(mcmodscanner_tests
            tests/test_core.cpp
            src/AppConfig.cpp
            src/AppConfig.h
            src/SearchTypes.cpp
            src/SearchTypes.h
            src/LicenseUtils.cpp
            src/LicenseUtils.h
            src/ScanCleaner.cpp
            src/ScanCleaner.h
            src/ExportService.cpp
            src/ExportService.h
        )
        target_link_libraries(mcmodscanner_tests PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Test
        )
        target_include_directories(mcmodscanner_tests PRIVATE src)
        add_test(NAME mcmodscanner_core COMMAND mcmodscanner_tests)
    else()
        message(STATUS "Qt6::Test not found; core tests are disabled.")
    endif()
endif()

option(MMS_BUILD_NETWORK_TESTS "Build optional live-network smoke tests" OFF)
if(MMS_BUILD_NETWORK_TESTS)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    qt_add_executable(mcmodscanner_network_tests
        tests/test_network.cpp
        src/AppConfig.cpp
        src/AppConfig.h
        src/SearchTypes.cpp
        src/SearchTypes.h
        src/HttpClient.cpp
        src/HttpClient.h
        src/BasePlatformClient.cpp
        src/BasePlatformClient.h
        src/ModrinthClient.cpp
        src/ModrinthClient.h
        src/CurseForgeClient.cpp
        src/CurseForgeClient.h
        src/NetworkTaskManager.cpp
        src/NetworkTaskManager.h
        src/McModWikiClient.cpp
        src/McModWikiClient.h
    )
    target_link_libraries(mcmodscanner_network_tests PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Test
    )
    target_include_directories(mcmodscanner_network_tests PRIVATE src)
    add_test(NAME mcmodscanner_network COMMAND mcmodscanner_network_tests)
    set_tests_properties(mcmodscanner_network PROPERTIES TIMEOUT 90)
endif()

option(MMS_BUILD_UI_TESTS "Build UI rendering smoke tests" OFF)
if(MMS_BUILD_UI_TESTS)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    qt_add_executable(mcmodscanner_ui_tests
        tests/test_ui.cpp
        ${APP_CORE_SOURCES}
    )
    qt_add_resources(mcmodscanner_ui_tests "ui_test_resources"
        PREFIX "/"
        BASE "resources"
        FILES
            resources/style.qss
            resources/app.svg
    )
    target_link_libraries(mcmodscanner_ui_tests PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Test
    )
    if(WIN32)
        target_link_libraries(mcmodscanner_ui_tests PRIVATE shell32)
    endif()
    target_compile_definitions(mcmodscanner_ui_tests PRIVATE
        MMS_VERSION="${PROJECT_VERSION}"
    )
    target_include_directories(mcmodscanner_ui_tests PRIVATE src)
    add_test(NAME mcmodscanner_ui COMMAND mcmodscanner_ui_tests)
    set_tests_properties(mcmodscanner_ui PROPERTIES TIMEOUT 60)
endif()
