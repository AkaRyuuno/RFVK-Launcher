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
    src/RemoteLoadLibrary.cpp src/RemoteLoadLibrary.h
    src/AutoLoginStore.cpp src/AutoLoginStore.h
    src/AutoLoginSession.cpp src/AutoLoginSession.h
    ${CMAKE_SOURCE_DIR}/autologin/rfvk_autologin_shared.h
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
target_include_directories(RFVKLauncher PRIVATE "${CMAKE_SOURCE_DIR}/autologin")

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

# ── 32-битная rfvk_autologin.dll (автовход внутри клиента) ───────────────────
# Клиент RF_Online.exe 32-битный, и библиотека, которая в нём работает, тоже
# должна быть x86. В одном дереве MSVC x64 и x86 не смешать, поэтому DLL —
# отдельный CMake-проект autologin/. Здесь он настраивается сразу (ошибки видны
# при настройке, а не посреди сборки) генератором Visual Studio с -A Win32 —
# тот сам находит x86-инструменты, консоль разработчика x86 и vcvarsall не нужны.
# Собирается вместе с лаунчером и кладётся рядом с ним.
option(RFVK_AUTOLOGIN "Собирать 32-битную rfvk_autologin.dll (автовход)" ON)

if(WIN32 AND RFVK_AUTOLOGIN)
    # Та же Visual Studio, что собирает лаунчер.
    if(CMAKE_GENERATOR MATCHES "^Visual Studio")
        set(_rfvk_x86_gen_default "${CMAKE_GENERATOR}")
        set(_rfvk_x86_inst_default "${CMAKE_GENERATOR_INSTANCE}")
    else()
        if(MSVC_VERSION GREATER_EQUAL 1950)
            set(_rfvk_x86_gen_default "Visual Studio 18 2026")
        elseif(MSVC_VERSION GREATER_EQUAL 1930)
            set(_rfvk_x86_gen_default "Visual Studio 17 2022")
        elseif(MSVC_VERSION GREATER_EQUAL 1920)
            set(_rfvk_x86_gen_default "Visual Studio 16 2019")
        endif()
        # <VS>/VC/Tools/MSVC/<версия>/bin/Hostx64/x64/cl.exe → <VS>
        string(REGEX REPLACE "/VC/Tools/MSVC/.*$" "" _rfvk_x86_inst_default "${CMAKE_CXX_COMPILER}")
        if(_rfvk_x86_inst_default STREQUAL CMAKE_CXX_COMPILER)
            set(_rfvk_x86_inst_default "")
        endif()
    endif()
    set(RFVK_X86_GENERATOR "${_rfvk_x86_gen_default}" CACHE STRING
        "Генератор Visual Studio для 32-битной rfvk_autologin.dll")
    set(RFVK_X86_VS_INSTANCE "${_rfvk_x86_inst_default}" CACHE PATH
        "Каталог Visual Studio для rfvk_autologin.dll (пусто — выберет CMake)")
    if(NOT RFVK_X86_GENERATOR)
        message(FATAL_ERROR
            "Не удалось определить генератор Visual Studio для rfvk_autologin.dll "
            "(компилятор: ${CMAKE_CXX_COMPILER}). Укажите -DRFVK_X86_GENERATOR=\"Visual Studio 17 2022\" "
            "или соберите без автовхода: -DRFVK_AUTOLOGIN=OFF.")
    endif()

    set(_rfvk_x86_dir "${CMAKE_BINARY_DIR}/autologin-x86")
    set(_rfvk_x86_args
        -S "${CMAKE_SOURCE_DIR}/autologin" -B "${_rfvk_x86_dir}"
        -G "${RFVK_X86_GENERATOR}" -A Win32
        "-DRFVK_OUTPUT_DIR=${CMAKE_BINARY_DIR}")
    if(RFVK_X86_VS_INSTANCE)
        list(APPEND _rfvk_x86_args "-DCMAKE_GENERATOR_INSTANCE=${RFVK_X86_VS_INSTANCE}")
    endif()
    # Окружение консоли x64 (Platform=x64 и т. п.) сбило бы проект Win32 — убираем.
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env --unset=Platform --unset=VSCMD_ARG_TGT_ARCH
                "${CMAKE_COMMAND}" ${_rfvk_x86_args}
        RESULT_VARIABLE _rfvk_x86_rc
        OUTPUT_VARIABLE _rfvk_x86_out
        ERROR_VARIABLE _rfvk_x86_out
    )
    if(NOT _rfvk_x86_rc EQUAL 0)
        message(FATAL_ERROR
            "Не удалось настроить 32-битную rfvk_autologin.dll (${RFVK_X86_GENERATOR}, -A Win32).
"
            "Нужна Visual Studio с компонентом «MSVC x86/x64» (C++ x86 build tools).
"
            "Собрать лаунчер без автовхода: -DRFVK_AUTOLOGIN=OFF.

${_rfvk_x86_out}")
    endif()

    set(_rfvk_autologin_dll "${CMAKE_BINARY_DIR}/rfvk_autologin.dll")
    # Всегда Release (/O2, /MT): отладочная DLL в клиенте не нужна. MSBuild сам
    # решает, что пересобрать; без изменений шаг занимает пару секунд.
    add_custom_target(rfvk_autologin ALL
        COMMAND "${CMAKE_COMMAND}" -E env --unset=Platform --unset=VSCMD_ARG_TGT_ARCH
                "${CMAKE_COMMAND}" --build "${_rfvk_x86_dir}" --config Release
        BYPRODUCTS "${_rfvk_autologin_dll}"
        COMMENT "Сборка 32-битной rfvk_autologin.dll (${RFVK_X86_GENERATOR}, Win32)"
        USES_TERMINAL
        VERBATIM
    )
    add_dependencies(RFVKLauncher rfvk_autologin)
    install(FILES "${_rfvk_autologin_dll}" DESTINATION .)
    set(_rfvk_autologin_summary "да — ${RFVK_X86_GENERATOR}, Win32 (${_rfvk_x86_dir})")
else()
    set(_rfvk_autologin_summary "нет (RFVK_AUTOLOGIN=OFF) — автовход в лаунчере будет недоступен")
endif()

message(STATUS "RFVKLauncher ${PROJECT_VERSION}: Qt ${Qt6_VERSION} (${Qt6_DIR})")
message(STATUS "  лаунчер (x64):            ${CMAKE_BINARY_DIR}/RFVKLauncher.exe")
message(STATUS "  rfvk_autologin.dll (x86): ${_rfvk_autologin_summary}")
message(STATUS "  seirin.dll (HUD):         не собирается — кладётся рядом с лаунчером вручную")

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
