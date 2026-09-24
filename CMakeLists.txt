cmake_minimum_required(VERSION 3.21)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SCRIPTURE_VERSION LIMIT_COUNT 1)
string(STRIP "${SCRIPTURE_VERSION}" SCRIPTURE_VERSION)
if(NOT SCRIPTURE_VERSION MATCHES "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$")
    message(FATAL_ERROR "VERSION must contain MAJOR.MINOR.PATCH")
endif()
string(REGEX MATCH "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$" _ "${SCRIPTURE_VERSION}")
set(SCRIPTURE_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(SCRIPTURE_VERSION_MINOR "${CMAKE_MATCH_2}")
set(SCRIPTURE_VERSION_PATCH "${CMAKE_MATCH_3}")
if(SCRIPTURE_VERSION_MAJOR GREATER 999 OR SCRIPTURE_VERSION_MINOR GREATER 999 OR SCRIPTURE_VERSION_PATCH GREATER 999)
    message(FATAL_ERROR "VERSION components must be at most 999")
endif()
math(EXPR SCRIPTURE_VERSION_CODE "(${SCRIPTURE_VERSION_MAJOR} * 1000000) + (${SCRIPTURE_VERSION_MINOR} * 1000) + ${SCRIPTURE_VERSION_PATCH}")
if(SCRIPTURE_VERSION_CODE LESS 1)
    message(FATAL_ERROR "Android versionCode must be positive")
endif()

project(scriptureandroid VERSION ${SCRIPTURE_VERSION} LANGUAGES CXX)

set(SCRIPTURE_OPENSSL_VERSION "3.0.22")
set(SCRIPTURE_OPENSSL_SOURCE_SHA256 "67ebca7e50d17383028045486653492195b83db95f8558709701bb47b5c1ef81")
set(SCRIPTURE_OPENSSL_API_LEVEL 28)
set(SCRIPTURE_ANDROID_NDK_VERSION "27.2.12479018")
set(SCRIPTURE_ANDROID_CLANG_VERSION "17.0.2")
string(LENGTH "${SCRIPTURE_OPENSSL_SOURCE_SHA256}" SCRIPTURE_OPENSSL_SOURCE_HASH_LENGTH)
if(NOT SCRIPTURE_OPENSSL_SOURCE_HASH_LENGTH EQUAL 64 OR NOT "${SCRIPTURE_OPENSSL_SOURCE_SHA256}" MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "OpenSSL source checksum is invalid")
endif()
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_android_openssl.sh" SCRIPTURE_OPENSSL_BUILD_SCRIPT)
set(SCRIPTURE_OPENSSL_SCRIPT_VERSION "")
set(SCRIPTURE_OPENSSL_SCRIPT_SHA256 "")
set(SCRIPTURE_OPENSSL_SCRIPT_API "")
set(SCRIPTURE_ANDROID_SCRIPT_NDK "")
string(REGEX MATCH "OPENSSL_VERSION=([0-9]+\\.[0-9]+\\.[0-9]+)" _ "${SCRIPTURE_OPENSSL_BUILD_SCRIPT}")
set(SCRIPTURE_OPENSSL_SCRIPT_VERSION "${CMAKE_MATCH_1}")
string(REGEX MATCH "OPENSSL_SHA256=([0-9a-f]+)" _ "${SCRIPTURE_OPENSSL_BUILD_SCRIPT}")
set(SCRIPTURE_OPENSSL_SCRIPT_SHA256 "${CMAKE_MATCH_1}")
string(REGEX MATCH "OPENSSL_API=([0-9]+)" _ "${SCRIPTURE_OPENSSL_BUILD_SCRIPT}")
set(SCRIPTURE_OPENSSL_SCRIPT_API "${CMAKE_MATCH_1}")
string(REGEX MATCH "NDK_VERSION=([0-9.]+)" _ "${SCRIPTURE_OPENSSL_BUILD_SCRIPT}")
set(SCRIPTURE_ANDROID_SCRIPT_NDK "${CMAKE_MATCH_1}")
if(NOT "${SCRIPTURE_OPENSSL_SCRIPT_VERSION}" STREQUAL "${SCRIPTURE_OPENSSL_VERSION}"
   OR NOT "${SCRIPTURE_OPENSSL_SCRIPT_SHA256}" STREQUAL "${SCRIPTURE_OPENSSL_SOURCE_SHA256}"
   OR NOT "${SCRIPTURE_OPENSSL_SCRIPT_API}" STREQUAL "${SCRIPTURE_OPENSSL_API_LEVEL}"
   OR NOT "${SCRIPTURE_ANDROID_SCRIPT_NDK}" STREQUAL "${SCRIPTURE_ANDROID_NDK_VERSION}")
    message(FATAL_ERROR "Android OpenSSL build metadata is inconsistent")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.9.0...6.11.99 REQUIRED COMPONENTS Core Gui Qml Quick Network QuickControls2)
if(QT_VERSION VERSION_GREATER_EQUAL "6.12.0")
    message(FATAL_ERROR "Qt 6.12 or newer is not supported")
endif()

qt_add_executable(scriptureandroid MANUAL_FINALIZATION
    src/main.cpp
    src/controller.cpp
    src/fetcher.cpp
    src/favorites.cpp
    src/references.cpp
    src/secrets.cpp
    src/updater.cpp
)

target_include_directories(scriptureandroid PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(scriptureandroid PRIVATE SCRIPTURE_VERSION="${SCRIPTURE_VERSION}")

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
    if(NOT "${ANDROID_NDK_VERSION}" STREQUAL "${SCRIPTURE_ANDROID_NDK_VERSION}")
        message(FATAL_ERROR "Android NDK ${SCRIPTURE_ANDROID_NDK_VERSION} is required")
    endif()
    if(NOT "${CMAKE_CXX_COMPILER_VERSION}" VERSION_EQUAL "${SCRIPTURE_ANDROID_CLANG_VERSION}")
        message(FATAL_ERROR "Android Clang ${SCRIPTURE_ANDROID_CLANG_VERSION} is required")
    endif()
    if(NOT ANDROID_ABI OR NOT CMAKE_ANDROID_ARCH_ABI OR NOT "${ANDROID_ABI}" STREQUAL "${CMAKE_ANDROID_ARCH_ABI}")
        message(FATAL_ERROR "Android ABI metadata is inconsistent")
    endif()
    set_target_properties(scriptureandroid PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "org.davidjm.scripture"
        QT_ANDROID_VERSION_CODE ${SCRIPTURE_VERSION_CODE}
        QT_ANDROID_VERSION_NAME "${SCRIPTURE_VERSION}"
        QT_ANDROID_MIN_SDK_VERSION ${SCRIPTURE_OPENSSL_API_LEVEL}
        QT_ANDROID_TARGET_SDK_VERSION 36
    )
    set(OPENSSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android/libs/${ANDROID_ABI}")
    set(OPENSSL_LIBRARIES
        "${OPENSSL_DIR}/libcrypto_3.so"
        "${OPENSSL_DIR}/libssl_3.so"
    )
    foreach(OPENSSL_LIBRARY IN LISTS OPENSSL_LIBRARIES)
        if(NOT EXISTS "${OPENSSL_LIBRARY}")
            message(FATAL_ERROR "Missing Android OpenSSL library: ${OPENSSL_LIBRARY}")
        endif()
    endforeach()
    if(CMAKE_READELF)
        set(SCRIPTURE_READELF "${CMAKE_READELF}")
    else()
        get_filename_component(SCRIPTURE_TOOLCHAIN_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
        find_program(SCRIPTURE_READELF NAMES llvm-readelf readelf HINTS "${SCRIPTURE_TOOLCHAIN_BIN}" REQUIRED)
    endif()
    find_program(SCRIPTURE_STRINGS NAMES llvm-strings strings REQUIRED)
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(SCRIPTURE_ELF_MACHINE "AArch64")
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(SCRIPTURE_ELF_MACHINE "X86-64")
    elseif(ANDROID_ABI STREQUAL "x86")
        set(SCRIPTURE_ELF_MACHINE "Intel 80386")
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(SCRIPTURE_ELF_MACHINE "ARM")
    else()
        message(FATAL_ERROR "Unsupported Android ABI: ${ANDROID_ABI}")
    endif()
    foreach(OPENSSL_LIBRARY IN LISTS OPENSSL_LIBRARIES)
        execute_process(COMMAND "${SCRIPTURE_READELF}" -h "${OPENSSL_LIBRARY}" RESULT_VARIABLE OPENSSL_READELF_RESULT OUTPUT_VARIABLE OPENSSL_HEADER ERROR_VARIABLE OPENSSL_READELF_ERROR)
        if(NOT OPENSSL_READELF_RESULT EQUAL 0 OR NOT OPENSSL_HEADER MATCHES "${SCRIPTURE_ELF_MACHINE}")
            message(FATAL_ERROR "OpenSSL ELF architecture mismatch for ${OPENSSL_LIBRARY}: ${OPENSSL_READELF_ERROR}")
        endif()
        execute_process(COMMAND "${SCRIPTURE_STRINGS}" -a "${OPENSSL_LIBRARY}" RESULT_VARIABLE OPENSSL_STRINGS_RESULT OUTPUT_VARIABLE OPENSSL_STRINGS ERROR_VARIABLE OPENSSL_STRINGS_ERROR)
        if(NOT OPENSSL_STRINGS_RESULT EQUAL 0 OR NOT OPENSSL_STRINGS MATCHES "OpenSSL ${SCRIPTURE_OPENSSL_VERSION}")
            message(FATAL_ERROR "OpenSSL version mismatch for ${OPENSSL_LIBRARY}: ${OPENSSL_STRINGS_ERROR}")
        endif()
    endforeach()
    execute_process(COMMAND "${SCRIPTURE_READELF}" -d "${OPENSSL_DIR}/libcrypto_3.so" RESULT_VARIABLE OPENSSL_CRYPTO_RESULT OUTPUT_VARIABLE OPENSSL_CRYPTO_DYNAMIC ERROR_VARIABLE OPENSSL_CRYPTO_ERROR)
    execute_process(COMMAND "${SCRIPTURE_READELF}" -d "${OPENSSL_DIR}/libssl_3.so" RESULT_VARIABLE OPENSSL_SSL_RESULT OUTPUT_VARIABLE OPENSSL_SSL_DYNAMIC ERROR_VARIABLE OPENSSL_SSL_ERROR)
    if(NOT OPENSSL_CRYPTO_RESULT EQUAL 0 OR NOT OPENSSL_CRYPTO_DYNAMIC MATCHES "SONAME.*\\[libcrypto_3\\.so\\]")
        message(FATAL_ERROR "libcrypto_3.so has an invalid SONAME: ${OPENSSL_CRYPTO_ERROR}")
    endif()
    if(NOT OPENSSL_SSL_RESULT EQUAL 0 OR NOT OPENSSL_SSL_DYNAMIC MATCHES "SONAME.*\\[libssl_3\\.so\\]" OR NOT OPENSSL_SSL_DYNAMIC MATCHES "NEEDED.*\\[libcrypto_3\\.so\\]" OR OPENSSL_SSL_DYNAMIC MATCHES "NEEDED.*\\[libcrypto\\.so\\]")
        message(FATAL_ERROR "libssl_3.so has invalid OpenSSL dependencies: ${OPENSSL_SSL_ERROR}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${OPENSSL_LIBRARIES})
    set_property(TARGET scriptureandroid APPEND PROPERTY QT_ANDROID_EXTRA_LIBS ${OPENSSL_LIBRARIES})
endif()

qt_finalize_executable(scriptureandroid)

include(CTest)
if(BUILD_TESTING)
    qt_add_executable(scripture_core_tests MANUAL_FINALIZATION
        tests/test_core.cpp
        src/fetcher.cpp
        src/references.cpp
        src/favorites.cpp
        src/secrets.cpp
    )
    target_include_directories(scripture_core_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(scripture_core_tests PRIVATE Qt6::Core Qt6::Network)
    qt_finalize_executable(scripture_core_tests)
    add_test(NAME scripture_core_tests COMMAND scripture_core_tests)
endif()
