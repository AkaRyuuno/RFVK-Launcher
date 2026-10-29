# rfvk_autologin.dll — библиотека автовхода, работает внутри клиента RF Online.
#
# Клиент 32-битный, поэтому это отдельный CMake-проект под Win32: в одном
# дереве сборки MSVC нельзя смешать x64 и x86. Корневой CMakeLists.txt
# настраивает его сам (генератор Visual Studio, -A Win32) и собирает вместе
# с лаунчером — руками запускать не нужно. Отдельно, если очень хочется:
#
#   cmake -S autologin -B out/build/autologin-x86 -G "Visual Studio 18 2026" -A Win32
#   cmake --build out/build/autologin-x86 --config Release

cmake_minimum_required(VERSION 3.21)

project(RFVKAutoLogin LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "rfvk_autologin.dll должна быть 32-битной (клиент RF Online — x86). "
        "Настройте этот проект генератором Visual Studio с -A Win32.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Статический CRT (/MT): библиотеке не нужен VC++ Redistributable у игрока.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_library(rfvk_autologin SHARED
    rfvk_autologin.cpp
    rfvk_autologin_shared.h
)
target_compile_definitions(rfvk_autologin PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_compile_options(rfvk_autologin PRIVATE /utf-8 /W3)
target_link_libraries(rfvk_autologin PRIVATE user32)

# Куда положить DLL — корневой проект передаёт каталог рядом с лаунчером.
# $<1:...> убирает подкаталог конфигурации (Release/), который добавляет Visual Studio.
if(RFVK_OUTPUT_DIR)
    set_target_properties(rfvk_autologin PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "$<1:${RFVK_OUTPUT_DIR}>"
    )
endif()
