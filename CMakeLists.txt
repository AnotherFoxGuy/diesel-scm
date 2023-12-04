cmake_minimum_required(VERSION 3.10)
if (${CMAKE_VERSION} VERSION_GREATER "3.14")
    cmake_policy(SET CMP0087 NEW)
endif ()
set(CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}" ${CMAKE_MODULE_PATH})
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

project(Diesel VERSION "2.0.0") 

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/ui")

find_package(Qt6 COMPONENTS Core Gui WebEngineCore WebEngineWidgets Core5Compat  REQUIRED)
find_package(Qt6Keychain REQUIRED)

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


find_package(Qt6LinguistTools)
#if (Qt6LinguistTools_FOUND)
if (0)
    set_source_files_properties(${TRANSLATIONS} PROPERTIES OUTPUT_LOCATION "intl")

    Qt6_create_translation(QM_MESSAGES ${CMAKE_SOURCE_DIR}/ui ${CMAKE_SOURCE_DIR}/src ${TRANSLATIONS})
    Qt6_add_translation(QM_FILES ${TRANSLATIONS})
    add_custom_target(messages DEPENDS ${QM_MESSAGES})
    add_custom_target(translations DEPENDS ${QM_FILES} messages)

    file(COPY rsrc/languages.qrc DESTINATION ${CMAKE_BINARY_DIR}/intl)
    list(APPEND RESOURCES "${CMAKE_BINARY_DIR}/intl/languages.qrc")
else ()
    message("Qt6LinguistTools not found")
endif ()

if (WIN32)
    # clang-cl doesn't support resource files
    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND SOURCES "${CMAKE_SOURCE_DIR}/rsrc/diesel.rc")
    endif ()
elseif (APPLE)
    list(APPEND SOURCES "${CMAKE_SOURCE_DIR}/rsrc/icons/diesel.icns")
endif ()

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
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::WebEngineCore Qt6::WebEngineWidgets Qt6::Core5Compat)
if (TARGET Qt6Keychain::Qt6Keychain)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6Keychain::Qt6Keychain)
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6keychain)
endif ()

target_compile_definitions(${PROJECT_NAME} PRIVATE DIESEL_VERSION="${PROJECT_VERSION}")

if (0)
    add_dependencies(${PROJECT_NAME} translations)
endif ()


# ------------------------------------------------------------------------------------------------#
# Cpack
# ------------------------------------------------------------------------------------------------#

include(GNUInstallDirs)
include(FetchContent)

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "${PROJECT_NAME}")
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

    get_target_property(Qt6_Core_Location Qt6::Core LOCATION)
    get_filename_component(QT_BIN_DIR ${Qt6_Core_Location} DIRECTORY)

    # Fix for windeployqt not being able to find Qt6keychain
    if (EXISTS ${RUNTIME_OUTPUT_DIRECTORY}/Qt6keychain.dll AND NOT EXISTS ${QT_BIN_DIR}/Qt6keychain.dll)
        message("Copying ${RUNTIME_OUTPUT_DIRECTORY}/Qt6keychain.dll to ${QT_BIN_DIR}")
        file(COPY ${RUNTIME_OUTPUT_DIRECTORY}/Qt6keychain.dll DESTINATION ${QT_BIN_DIR})
    endif ()

    add_custom_target(
            copy_dll
            COMMAND ${QT_BIN_DIR}/windeployqt.exe $<TARGET_FILE:${PROJECT_NAME}> --release --no-compiler-runtime --no-opengl-sw
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    install(
            TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION .
    )

    install(CODE "execute_process(COMMAND ${QT_BIN_DIR}/windeployqt.exe $<TARGET_FILE:${PROJECT_NAME}> --release --no-opengl-sw --dir \${CMAKE_INSTALL_PREFIX})")


    FetchContent_Declare(
            fossil
            URL https://fossil-scm.org/home/uv/fossil-w64-2.22.zip
            URL_MD5 08795c21fb2afd172ecec7fce6de8f43
    )
    FetchContent_MakeAvailable(fossil)

    install(
            PROGRAMS ${fossil_SOURCE_DIR}/fossil.exe
            DESTINATION .
            COMPONENT Fossil
            EXCLUDE_FROM_ALL
    )

    # For Windows Desktop shortcuts
    set(CPACK_CREATE_DESKTOP_LINKS "${PROJECT_NAME}" "${PROJECT_NAME}")

    set(CPACK_INNOSETUP_ALLOW_CUSTOM_DIRECTORY ON)
    set(CPACK_INNOSETUP_USE_MODERN_WIZARD ON)
    set(CPACK_INNOSETUP_ICON_FILE "${CMAKE_SOURCE_DIR}/rsrc/icons/diesel.ico")

    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
    set(CPACK_GENERATOR ${CPACK_GENERATOR} INNOSETUP)
endif ()

if (UNIX)
    install(
            TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(
            FILES ${CMAKE_SOURCE_DIR}/rsrc/diesel.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications/
    )
    install(
            FILES ${CMAKE_SOURCE_DIR}/rsrc/icons/diesel.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps/
    )
endif ()

include(CPack)

feature_summary(WHAT ALL)