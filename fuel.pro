#-------------------------------------------------
# Fuel
#-------------------------------------------------

QT    += core gui

TARGET = Fuel
TEMPLATE = app

# OSX Icon
ICON = rsrc/icons/fuel.icns

# Win Icon
RC_FILE = rsrc/fuel.rc

INCLUDEPATH += src

SOURCES += src/main.cpp\
		src/MainWindow.cpp \
	src/CommitDialog.cpp \
	src/FileActionDialog.cpp \
	src/SettingsDialog.cpp \
	src/Utils.cpp \
	src/FileTableView.cpp \
	src/CloneDialog.cpp \
	src/LoggedProcess.cpp

HEADERS  += src/MainWindow.h \
	src/CommitDialog.h \
	src/FileActionDialog.h \
	src/SettingsDialog.h \
	src/Utils.h \
	src/FileTableView.h \
	src/CloneDialog.h \
	src/LoggedProcess.h

FORMS    += ui/MainWindow.ui \
	ui/CommitDialog.ui \
	ui/FileActionDialog.ui \
	ui/SettingsDialog.ui \
	ui/CloneDialog.ui

RESOURCES += \
	rsrc/resources.qrc

win32 {
	LIBS += -luser32 -lshell32
}
