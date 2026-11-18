# rfvk_proxy.dll — библиотека перехвата сетевого трафика в клиенте RF Online.
#
# Клиент 32-битный, поэтому это отдельный CMake-проект под Win32: в одном
# дереве сборки MSVC нельзя смешать x64 и x86. Корневой CMakeLists.txt
# настраивает его сам (генератор Visual Studio, -A Win32) и собирает вместе
# с лаунчером — руками запускать не нужно. Отдельно, если очень хочется:
#
#   cmake -S proxy -B out/build/proxy-x86 -G "Visual Studio 18 2026" -A Win32
#   cmake --build out/build/proxy-x86 --config Release

cmake_minimum_required(VERSION 3.21)

project(RFVKProxy LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "rfvk_proxy.dll должна быть 32-битной (клиент RF Online — x86). "
        "Настройте этот проект генератором Visual Studio с -A Win32.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Статический CRT (/MT): библиотеке не нужен VC++ Redistributable у игрока.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_library(rfvk_proxy SHARED
    rfvk_proxy.cpp
    rfvk_proxy_shared.h
)
target_compile_definitions(rfvk_proxy PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_compile_options(rfvk_proxy PRIVATE /utf-8 /W3)
# iphlpapi — проверка соединений клиента, открытых до перехвата.
target_link_libraries(rfvk_proxy PRIVATE ws2_32 iphlpapi)

# Куда положить DLL — корневой проект передаёт каталог рядом с лаунчером.
# $<1:...> убирает подкаталог конфигурации (Release/), который добавляет Visual Studio.
if(RFVK_OUTPUT_DIR)
    set_target_properties(rfvk_proxy PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "$<1:${RFVK_OUTPUT_DIR}>"
    )
endif()
