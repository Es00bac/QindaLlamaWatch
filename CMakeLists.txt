# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.24)

# AGENT-NOTE: this version, the Portage ebuild version, and the git tag move
# together; the ebuild pins the commit it builds (pattern: QindaMPV).
project(qindallamawatch
    VERSION 0.1.0
    DESCRIPTION "QindaLlamaWatch - live monitor for a remote llama.cpp server on the QindaQt desktop"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)
include(CTest)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 DBus)
qt_standard_project_setup(REQUIRES 6.8)

# The QindaQt desktop ships these archives/headers without CMake package
# configs (owned by gui-wm/qindaqt-desktop), so resolve them explicitly.
# libqindaqt_app_shell.so is the backing library of the installed
# QindaQt.AppShell QML module and lives in the module directory.
find_library(QINDAQT_APP_SHELL_LIB NAMES qindaqt_app_shell
    PATH_SUFFIXES qt6/qml/QindaQt/AppShell)
find_library(QINDAQT_MENU_EXPORT_LIB NAMES qindaqt_app_shell_menu_export)
find_library(QINDAQT_MENU_DBUSMENU_LIB NAMES qindaqt_global_menu_dbusmenu)
find_library(QINDAQT_MENU_REGISTRAR_LIB NAMES qindaqt_global_menu_registrar)
find_library(QINDAQT_MENU_EXPORTER_LIB NAMES qindaqt_global_menu_exporter)
find_library(QINDAQT_MENU_OWNERSHIP_LIB NAMES qindaqt_global_menu_ownership)
find_library(QINDAQT_MENU_PROTOCOL_LIB NAMES qindaqt_global_menu_protocol)
find_path(QINDAQT_APP_SHELL_INCLUDE_DIR qindaqt/app_shell/application_coordinator.h)
find_path(QINDAQT_MENU_EXPORT_INCLUDE_DIR qindaqt/app_shell/menu_export/first_party_composition.h)

foreach(dependency IN ITEMS
        QINDAQT_APP_SHELL_LIB QINDAQT_MENU_EXPORT_LIB QINDAQT_MENU_DBUSMENU_LIB
        QINDAQT_MENU_REGISTRAR_LIB QINDAQT_MENU_EXPORTER_LIB QINDAQT_MENU_OWNERSHIP_LIB
        QINDAQT_MENU_PROTOCOL_LIB QINDAQT_APP_SHELL_INCLUDE_DIR QINDAQT_MENU_EXPORT_INCLUDE_DIR)
    if(NOT ${dependency})
        message(FATAL_ERROR "QindaLlamaWatch requires the QindaQt desktop libraries (${dependency}). "
                            "Install gui-wm/qindaqt-desktop first.")
    endif()
endforeach()

cmake_path(GET QINDAQT_APP_SHELL_LIB PARENT_PATH QINDAQT_APP_SHELL_LIB_DIR)

# The data collectors are a plain library so the parsers and rate maths are
# tested without QML, a window, a network or a remote host.
add_library(llamawatch_core STATIC
    src/common/remotecommand.cpp
    src/common/remotecommand.h
    src/common/streamprocess.cpp
    src/common/streamprocess.h
    src/host/hostprobe.cpp
    src/host/hostprobe.h
    src/host/hostsampleparser.cpp
    src/host/hostsampleparser.h
    src/host/hoststats.cpp
    src/host/hoststats.h
    src/journal/journalfeed.cpp
    src/journal/journalfeed.h
    src/journal/journalparser.cpp
    src/journal/journalparser.h
    src/journal/serverjournal.cpp
    src/journal/serverjournal.h
    src/llama/llamaclient.cpp
    src/llama/llamaclient.h
    src/llama/metricsparser.cpp
    src/llama/metricsparser.h
    src/llama/slotrates.cpp
    src/llama/slotrates.h
    src/settings/monitorsettings.cpp
    src/settings/monitorsettings.h
)
# AGENT-CONTRACT: sampler.sh is read from :/llamawatch/sampler.sh by
# HostProbe and piped to the remote shell; the alias is that path.
qt_add_resources(llamawatch_core "sampler"
    PREFIX /llamawatch
    BASE src/host
    FILES src/host/sampler.sh)
target_include_directories(llamawatch_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(llamawatch_core PUBLIC QT_NO_CAST_FROM_ASCII)
target_link_libraries(llamawatch_core PUBLIC Qt6::Core Qt6::Network)

qt_add_executable(qindallamawatch
    src/main.cpp
    src/app/action_catalog.cpp
    src/app/action_catalog.h
    src/app/crashnotifier.cpp
    src/app/crashnotifier.h
)

qt_add_qml_module(qindallamawatch
    URI QindaLlamaWatch
    VERSION 1.0
    # AGENT-GUARD: without this prefix the installed binary fails with
    # "module QindaLlamaWatch contains no type Main" (see QindaMPV).
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Main.qml
        qml/MonitorMenuBar.qml
        qml/DashboardView.qml
        qml/ModelPanel.qml
        qml/ThroughputPanel.qml
        qml/SlotsPanel.qml
        qml/SystemPanel.qml
        qml/MeterRow.qml
        qml/TraceGraph.qml
        qml/RequestsPanel.qml
        qml/LogView.qml
        qml/SettingsDialog.qml
        qml/CrashBanner.qml
        qml/Format.js
)

target_include_directories(qindallamawatch PRIVATE
    ${QINDAQT_APP_SHELL_INCLUDE_DIR}
    ${QINDAQT_MENU_EXPORT_INCLUDE_DIR}
)
target_compile_definitions(qindallamawatch PRIVATE
    LLAMAWATCH_VERSION="${PROJECT_VERSION}"
)
set_target_properties(qindallamawatch PROPERTIES
    INSTALL_RPATH "${QINDAQT_APP_SHELL_LIB_DIR}"
)

# Static-archive order matters: dependents before dependencies.
target_link_libraries(qindallamawatch PRIVATE
    llamawatch_core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
    Qt6::DBus
    Qt6::Gui
    ${QINDAQT_MENU_EXPORT_LIB}
    ${QINDAQT_MENU_DBUSMENU_LIB}
    ${QINDAQT_MENU_REGISTRAR_LIB}
    ${QINDAQT_MENU_EXPORTER_LIB}
    ${QINDAQT_MENU_OWNERSHIP_LIB}
    ${QINDAQT_MENU_PROTOCOL_LIB}
    ${QINDAQT_APP_SHELL_LIB}
)

install(TARGETS qindallamawatch RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/org.qindaqt.LlamaWatch.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/icons/hicolor/scalable/apps/org.qindaqt.LlamaWatch.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
