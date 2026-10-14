cmake_minimum_required(VERSION 3.21)

project(RFVKLauncher VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Нужен Qt 6.5+ (проверено на 6.11.2 msvc2022_64). Сеть не требуется: бэкенд — заглушка.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Svg Network Sql)
qt_standard_project_setup(REQUIRES 6.5)

add_subdirectory(RFVKLauncher)
