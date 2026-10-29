qt_add_executable(RFVKLauncher
    src/main.cpp
    src/Account.h
    src/AccountModel.cpp src/AccountModel.h
    src/IAuthBackend.h
    src/StubAuthBackend.cpp src/StubAuthBackend.h
    src/VkCrypto.cpp src/VkCrypto.h
    src/VkChromeCookies.cpp src/VkChromeCookies.h
    src/VkPlayAuthBackend.cpp src/VkPlayAuthBackend.h
    src/HudInjector.cpp src/HudInjector.h
    src/AutoLoginStore.cpp src/AutoLoginStore.h
    src/AutoLoginSession.cpp src/AutoLoginSession.h
    ${CMAKE_SOURCE_DIR}/injector/rfvk_autologin_shared.h
    src/Controller.cpp src/Controller.h
)

# Theme и Strings — QML-синглтоны (токены дизайна и тексты интерфейса).
set_source_files_properties(qml/Theme.qml qml/Strings.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(RFVKLauncher
    URI Rfvk
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Strings.qml
        qml/Icon.qml
        qml/Sidebar.qml
        qml/AccountCard.qml
        qml/AccountPanel.qml
        qml/EmptyState.qml
        qml/GameDirRow.qml
        qml/LogConsole.qml
        qml/AddAccountDialog.qml
        qml/ConfirmDialog.qml
        qml/Toasts.qml
        qml/MultiPanel.qml
        qml/SettingsDialog.qml
        qml/components/AppText.qml
        qml/components/AppToggle.qml
        qml/components/PrimaryButton.qml
        qml/components/SecondaryButton.qml
        qml/components/StatusBadge.qml
        qml/components/Spinner.qml
        qml/components/AppField.qml
        qml/components/AppSelect.qml
        qml/AutoLoginCard.qml
    RESOURCES
        resources/app.png
        resources/fonts/Exo2-Variable.ttf
        resources/fonts/GolosText-Variable.ttf
        resources/fonts/IBMPlexMono-Regular.ttf
        resources/fonts/IBMPlexMono-Medium.ttf
        resources/fonts/IBMPlexMono-SemiBold.ttf
)

target_compile_definitions(RFVKLauncher PRIVATE RFVK_VERSION="${PROJECT_VERSION}")
# Формат общей памяти автовхода — общий с 32-битной rfvk_autologin.dll.
target_include_directories(RFVKLauncher PRIVATE "${CMAKE_SOURCE_DIR}/injector")

set_target_properties(RFVKLauncher PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME RFVKLauncher
)

target_link_libraries(RFVKLauncher PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
    Qt6::Network
    Qt6::Sql
)
if(WIN32)
    target_link_libraries(RFVKLauncher PRIVATE crypt32 bcrypt)
    # Иконка .exe — эмблема-октагон из верхней полосы (resources/app.ico).
    target_sources(RFVKLauncher PRIVATE resources/app.rc)
endif()

# ── 32-битные помощники: инжектор (rfvk_inject.exe) и автовход (rfvk_autologin.dll)
# Лаунчер 64-битный, клиент RF_Online.exe — 32-битный; LoadLibrary-инжект
# работает только из процесса той же разрядности, поэтому инжектор собирается
# отдельной 32-битной целью рядом с лаунчером. Это самодостаточный Win32-файл
# без Qt: компилируем его напрямую через cl в окружении x86 (см.
# injector/build-injector.bat). Если x86-тулчейн не найден — пропускаем
# (лаунчер соберётся, автоподключение HUD просто будет неактивно).
if(WIN32)
    if(DEFINED ENV{VCINSTALLDIR})
        file(TO_NATIVE_PATH "$ENV{VCINSTALLDIR}Auxiliary/Build/vcvarsall.bat" _rfvk_vcvarsall_default)
    endif()
    set(RFVK_VCVARSALL "${_rfvk_vcvarsall_default}" CACHE FILEPATH
        "vcvarsall.bat для сборки 32-битного rfvk_inject.exe")
    # Кэш не перезаписывается сам: если первая настройка была вне консоли
    # разработчика (путь пустой/неверный), подхватываем его теперь. Заданный
    # вручную существующий путь не трогаем.
    if((NOT RFVK_VCVARSALL OR NOT EXISTS "${RFVK_VCVARSALL}")
       AND _rfvk_vcvarsall_default AND EXISTS "${_rfvk_vcvarsall_default}")
        set(RFVK_VCVARSALL "${_rfvk_vcvarsall_default}" CACHE FILEPATH
            "vcvarsall.bat для сборки 32-битного rfvk_inject.exe" FORCE)
    endif()

    set(_rfvk_inject_src "${CMAKE_SOURCE_DIR}/injector/rfvk_inject.cpp")
    set(_rfvk_inject_bat "${CMAKE_SOURCE_DIR}/injector/build-injector.bat")
    set(_rfvk_inject_exe "${CMAKE_BINARY_DIR}/rfvk_inject.exe")
    set(_rfvk_autologin_src "${CMAKE_SOURCE_DIR}/injector/rfvk_autologin.cpp")
    set(_rfvk_autologin_hdr "${CMAKE_SOURCE_DIR}/injector/rfvk_autologin_shared.h")
    set(_rfvk_autologin_dll "${CMAKE_BINARY_DIR}/rfvk_autologin.dll")

    if(RFVK_VCVARSALL AND EXISTS "${RFVK_VCVARSALL}")
        add_custom_command(
            OUTPUT "${_rfvk_inject_exe}"
            COMMAND "${_rfvk_inject_bat}" "${_rfvk_inject_src}" "${CMAKE_BINARY_DIR}" "${RFVK_VCVARSALL}"
            DEPENDS "${_rfvk_inject_src}" "${_rfvk_inject_bat}"
            COMMENT "Сборка 32-битного rfvk_inject.exe"
            VERBATIM
        )
        add_custom_target(rfvk_inject ALL DEPENDS "${_rfvk_inject_exe}")
        add_dependencies(RFVKLauncher rfvk_inject)
        install(PROGRAMS "${_rfvk_inject_exe}" DESTINATION .)

        # Автовход в клиенте: библиотека подключается тем же инжектором.
        add_custom_command(
            OUTPUT "${_rfvk_autologin_dll}"
            COMMAND "${_rfvk_inject_bat}" "${_rfvk_autologin_src}" "${CMAKE_BINARY_DIR}" "${RFVK_VCVARSALL}" dll rfvk_autologin
            DEPENDS "${_rfvk_autologin_src}" "${_rfvk_autologin_hdr}" "${_rfvk_inject_bat}"
            COMMENT "Сборка 32-битной rfvk_autologin.dll"
            VERBATIM
        )
        add_custom_target(rfvk_autologin ALL DEPENDS "${_rfvk_autologin_dll}")
        add_dependencies(RFVKLauncher rfvk_autologin)
        install(FILES "${_rfvk_autologin_dll}" DESTINATION .)
    else()
        message(WARNING "RFVK_VCVARSALL не задан/не найден — rfvk_inject.exe не будет собран, "
                        "автоподключение HUD и автовход отключены. Укажите -DRFVK_VCVARSALL=<путь к vcvarsall.bat>.")
    endif()
endif()

# Поставка: cmake --install <build> --prefix dist (windeployqt для QML-импортов).
install(TARGETS RFVKLauncher RUNTIME DESTINATION .)
qt_generate_deploy_qml_app_script(
    TARGET RFVKLauncher
    OUTPUT_SCRIPT deploy_script
    NO_TRANSLATIONS
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(CODE "set(QT_DEPLOY_BIN_DIR .)")
install(SCRIPT ${deploy_script})
