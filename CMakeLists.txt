cmake_minimum_required(VERSION 3.21)
project(anomaly-spotter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT MSVC)
    add_compile_options(-Wall)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

qt_standard_project_setup()

set(TRAINER_CORE_SOURCES
    src/MainWindow.cpp
    src/MainWindow.h
    src/ScreenCaptureService.cpp
    src/ScreenCaptureService.h
    src/DiffEngine.cpp
    src/DiffEngine.h
    src/OverlayWindow.cpp
    src/OverlayWindow.h
    src/HudOverlay.cpp
    src/HudOverlay.h
    src/GlobalHotkeys.cpp
    src/GlobalHotkeys.h
)

if(WIN32)
    list(APPEND TRAINER_CORE_SOURCES src/WindowsHotkeys.cpp src/WindowsHotkeys.h)
else()
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    find_package(LayerShellQt QUIET)
    find_package(X11)
    list(APPEND TRAINER_CORE_SOURCES src/PortalHotkeys.cpp src/PortalHotkeys.h)
    if(X11_FOUND AND TARGET X11::xcb)
        list(APPEND TRAINER_CORE_SOURCES src/X11Hotkeys.cpp src/X11Hotkeys.h)
    endif()
endif()

qt_add_library(trainer-core STATIC ${TRAINER_CORE_SOURCES})
target_include_directories(trainer-core PUBLIC src)
target_link_libraries(trainer-core PUBLIC Qt6::Widgets Qt6::Multimedia)

if(WIN32)
    target_link_libraries(trainer-core PUBLIC user32)
else()
    target_link_libraries(trainer-core PUBLIC Qt6::DBus)
    if(LayerShellQt_FOUND)
        target_link_libraries(trainer-core PUBLIC LayerShellQt::Interface)
        target_compile_definitions(trainer-core PRIVATE HAVE_LAYER_SHELL)
    endif()
    if(X11_FOUND AND TARGET X11::xcb)
        target_compile_definitions(trainer-core PRIVATE HAVE_X11_HOTKEYS)
        target_link_libraries(trainer-core PUBLIC X11::X11 X11::xcb)
    endif()
endif()

set(APP_SOURCES src/main.cpp resources.qrc)
if(WIN32)
    list(APPEND APP_SOURCES assets/app.rc)
endif()

qt_add_executable(anomaly-spotter ${APP_SOURCES})
target_link_libraries(anomaly-spotter PRIVATE trainer-core)
if(WIN32)
    set_target_properties(anomaly-spotter PROPERTIES WIN32_EXECUTABLE TRUE)
    set_source_files_properties(assets/app.rc PROPERTIES
        INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/assets")
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS anomaly-spotter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES assets/anomaly-spotter.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES assets/icon.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
            RENAME anomaly-spotter.png)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)

    qt_add_executable(test-scene tests/scene-window/main.cpp)
    target_link_libraries(test-scene PRIVATE Qt6::Widgets)

    qt_add_executable(tst_diffengine tests/unit/tst_diffengine.cpp)
    target_link_libraries(tst_diffengine PRIVATE trainer-core Qt6::Test)
    add_test(NAME diffengine COMMAND tst_diffengine)

    qt_add_executable(tst_capture tests/unit/tst_capture.cpp)
    target_link_libraries(tst_capture PRIVATE trainer-core Qt6::Test)
    add_test(NAME capture COMMAND tst_capture)

    qt_add_executable(tst_trainer tests/unit/tst_trainer.cpp)
    target_link_libraries(tst_trainer PRIVATE trainer-core Qt6::Test)
    add_test(NAME trainer COMMAND tst_trainer)

    set_tests_properties(diffengine capture trainer PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
