cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0087 NEW)
include(FeatureSummary)

# Setup paths
SET(RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")
SET(LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/")
SET(ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/")
SET(EXECUTABLE_OUTPUT_PATH ${RUNTIME_OUTPUT_DIRECTORY})
SET(LIBRARY_OUTPUT_PATH ${RUNTIME_OUTPUT_DIRECTORY})
# Fix executable paths for windows
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${RUNTIME_OUTPUT_DIRECTORY})
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${RUNTIME_OUTPUT_DIRECTORY})
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${RUNTIME_OUTPUT_DIRECTORY})
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${RUNTIME_OUTPUT_DIRECTORY})

project(Fuel VERSION "2.0.1")

option(UPDATE_TRANSLATIONS "Update translations" OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/ui")

find_package(Qt5 COMPONENTS Core Gui WebEngineWidgets REQUIRED)

set(FORMS
        ui/MainWindow.ui
        ui/CommitDialog.ui
        ui/FileActionDialog.ui
        ui/SettingsDialog.ui
        ui/FslSettingsDialog.ui
        ui/CloneDialog.ui
        ui/BrowserWidget.ui
        ui/RevisionDialog.ui
        ui/RemoteDialog.ui
        ui/AboutDialog.ui)

set(SOURCES
        src/main.cpp
        src/MainWindow.cpp
        src/CommitDialog.cpp
        src/FileActionDialog.cpp
        src/SettingsDialog.cpp
        src/FslSettingsDialog.cpp
        src/CloneDialog.cpp
        src/RevisionDialog.cpp
        src/Utils.cpp
        src/FileTableView.cpp
        src/LoggedProcess.cpp
        src/BrowserWidget.cpp
        src/CustomWebView.cpp
        src/Fossil.cpp
        src/Workspace.cpp
        src/SearchBox.cpp
        src/AppSettings.cpp
        src/RemoteDialog.cpp
        src/AboutDialog.cpp)

set(HEADERS
        src/MainWindow.h
        src/CommitDialog.h
        src/FileActionDialog.h
        src/SettingsDialog.h
        src/FslSettingsDialog.h
        src/CloneDialog.h
        src/RevisionDialog.h
        src/Utils.h
        src/FileTableView.h
        src/LoggedProcess.h
        src/BrowserWidget.h
        src/CustomWebView.h
        src/Fossil.h
        src/Workspace.h
        src/SearchBox.h
        src/AppSettings.h
        src/RemoteDialog.h
        src/AboutDialog.h
        src/WorkspaceCommon.h)


set(RESOURCES
        rsrc/resources.qrc)

set(TRANSLATIONS
        intl/en_US.ts
        intl/el_GR.ts
        intl/de_DE.ts
        intl/es_ES.ts
        intl/fr_FR.ts
        intl/ru_RU.ts
        intl/pt_PT.ts
        intl/it_IT.ts
        intl/nl_NL.ts
        intl/ko_KR.ts)

set_source_files_properties(${TRANSLATIONS} PROPERTIES OUTPUT_LOCATION "${RUNTIME_OUTPUT_DIRECTORY}translations")

find_package(Qt5LinguistTools)
if (Qt5LinguistTools_FOUND)
    if (UPDATE_TRANSLATIONS)
        qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR}/ui ${CMAKE_SOURCE_DIR}/src ${TRANSLATIONS})
    else ()
        qt5_add_translation(QM_FILES ${TRANSLATIONS})
    endif ()
    install(FILES ${QM_FILES} DESTINATION ./translations)
else ()
    message("Qt5LinguistTools not found")
endif ()

if (WIN32)
    # clang-cl doesn't support resource files
    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND SOURCES "${CMAKE_SOURCE_DIR}/rsrc/fuel.rc")
    endif ()
elseif (APPLE)
    list(APPEND SOURCES "${CMAKE_SOURCE_DIR}/rsrc/icons/fuel.icns")
endif ()

add_subdirectory("${CMAKE_SOURCE_DIR}/ext/qtkeychain" EXCLUDE_FROM_ALL)

add_executable(
        ${PROJECT_NAME}
        ${FORMS}
        ${HEADERS}
        ${SOURCES}
        ${QM_FILES}
        ${RESOURCES}
)

target_include_directories(
        ${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/ui/
        ${CMAKE_SOURCE_DIR}/src/

)
target_link_libraries(${PROJECT_NAME} Qt5::Widgets Qt5::WebEngineCore Qt5::WebEngineWidgets qt5keychain)

# ------------------------------------------------------------------------------------------------#
# Cpack
# ------------------------------------------------------------------------------------------------#

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "${PROJECT_NAME}")
set(CPACK_PACKAGE_DESCRIPTION "A GUI front-end for the Fossil SCM")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A GUI front-end for the Fossil SCM")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/rsrc/license.rtf")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME}" "${PROJECT_NAME}")

set(CPACK_GENERATOR ZIP)

if (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)

    get_target_property(Qt5_Core_Location Qt5::Core LOCATION)
    get_filename_component(QT_BIN_DIR ${Qt5_Core_Location} DIRECTORY)

    add_custom_target(
            copy_dll
            COMMAND ${QT_BIN_DIR}/windeployqt.exe $<TARGET_FILE:${PROJECT_NAME}> --release --no-compiler-runtime --no-translations --no-opengl-sw
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    install(
            TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION .
    )

    install(CODE "execute_process(COMMAND ${QT_BIN_DIR}/windeployqt.exe $<TARGET_FILE:${PROJECT_NAME}> --release --no-compiler-runtime --no-translations --no-opengl-sw --dir \${CMAKE_INSTALL_PREFIX})")

    # For Windows Desktop shortcuts
    set(CPACK_CREATE_DESKTOP_LINKS "${PROJECT_NAME}" "${PROJECT_NAME}")

    # Windows Add or Remove Program properties
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "${PROJECT_NAME}")
    set(CPACK_WIX_PROPERTY_ARPCOMMENTS "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}")
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "https://fuel-scm.org")
    set(CPACK_WIX_PROPERTY_URLUPDATEINFO "https://fuel-scm.org")

    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/rsrc/icons/fuel.ico")

    set(CPACK_WIX_LICENSE_RTF "${CPACK_RESOURCE_FILE_LICENSE}")
    set(CPACK_WIX_UPGRADE_GUID "f33967d1-0bdd-4a4b-ba1a-d859bce8927a")

    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
    set(CPACK_GENERATOR ${CPACK_GENERATOR};WIX)
    set(CPACK_MODULE_PATH "")
endif ()
include(CPack)

feature_summary(WHAT ALL)