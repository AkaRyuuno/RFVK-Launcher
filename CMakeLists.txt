cmake_minimum_required(VERSION 3.21)

# Версия: по умолчанию отсюда, сборка релиза в CI передаёт её из тега
# (1.2.3 → -DRFVK_VERSION=1.2.3).
set(RFVK_VERSION "0.1.0" CACHE STRING "Версия лаунчера (X.Y.Z)")
if(NOT RFVK_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "RFVK_VERSION='${RFVK_VERSION}' — нужен формат X.Y.Z (например 0.2.0)")
endif()
project(RFVKLauncher VERSION ${RFVK_VERSION} LANGUAGES CXX)

# Лаунчер — x64 (готовый Qt 6 для MSVC есть только 64-битный). 32-битная
# часть (rfvk_autologin.dll) собирается отдельно — см. RFVKLauncher/CMakeLists.txt.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Лаунчер собирается только под x64: запустите CMake из консоли "
                        "разработчика x64 (vcvars64.bat) или выберите пресет *-x64.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Нужен Qt 6.5+ (проверено на 6.11.2 msvc2022_64). Сеть не требуется: бэкенд — заглушка.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Svg Network Sql)
qt_standard_project_setup(REQUIRES 6.5)

add_subdirectory(RFVKLauncher)
