cmake_minimum_required(VERSION 3.21)
project(scriptureandroid VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network QuickControls2)

qt_add_executable(scriptureandroid MANUAL_FINALIZATION
    src/main.cpp
    src/controller.cpp
    src/fetcher.cpp
    src/favorites.cpp
    src/references.cpp
    src/updater.cpp
)

target_include_directories(scriptureandroid PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(scriptureandroid PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
    Qt6::QuickControls2
)

qt_add_resources(scriptureandroid "qml"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/qml/main.qml"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/qml/ScriptureSettings.qml"
)

if(ANDROID)
    set_target_properties(scriptureandroid PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_VERSION_CODE 1
        QT_ANDROID_VERSION_NAME "1.0"
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 36
    )
endif()

qt_finalize_executable(scriptureandroid)