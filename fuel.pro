#-------------------------------------------------
#
# Project created by QtCreator 2011-08-01T00:17:18
#
#-------------------------------------------------

QT    += core gui

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
	SettingsDialog.cpp \
	Utils.cpp \
	FileTableView.cpp \
	CloneDialog.cpp \
    LoggedProcess.cpp

HEADERS  += MainWindow.h \
	CommitDialog.h \
	FileActionDialog.h \
	SettingsDialog.h \
	Utils.h \
	FileTableView.h \
	CloneDialog.h \
    LoggedProcess.h

FORMS    += MainWindow.ui \
	CommitDialog.ui \
	FileActionDialog.ui \
	SettingsDialog.ui \
	CloneDialog.ui

RESOURCES += \
	resources.qrc

win32 {
	LIBS += -luser32 -lshell32
}
