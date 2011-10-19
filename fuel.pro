#-------------------------------------------------
#
# Project created by QtCreator 2011-08-01T00:17:18
#
#-------------------------------------------------

QT       += core gui

TARGET = Fuel
TEMPLATE = app

# OSX Icon
ICON = icons/fuel.icns

# Win Icon
RC_FILE = fuel.rc

SOURCES += main.cpp\
        MainWindow.cpp \
    CommitDialog.cpp \
    FileActionDialog.cpp \
    SettingsDialog.cpp

HEADERS  += MainWindow.h \
    CommitDialog.h \
    FileActionDialog.h \
    SettingsDialog.h

FORMS    += MainWindow.ui \
    CommitDialog.ui \
    FileActionDialog.ui \
    SettingsDialog.ui

RESOURCES += \
    resources.qrc




















