#-------------------------------------------------
# Fuel
#-------------------------------------------------

QT    += core gui webkit

contains(QT_VERSION, ^5\\..*) {
	QT += widgets webkitwidgets
}

TARGET = Fuel
TEMPLATE = app

win32 {
	RC_FILE = rsrc/fuel.rc
	LIBS += -luser32 -lshell32 -luuid
}

macx {
	ICON = rsrc/icons/fuel.icns
}

unix:!macx {
	TARGET = fuel
	ICON = rsrc/icons/fuel.png
	PREFIX = /usr
	BINDIR = $$PREFIX/bin
	DATADIR = $$PREFIX/share
	target.path = $$BINDIR

	desktop.path = $$DATADIR/applications
	desktop.files += rsrc/fuel.desktop

	icon.path = $$DATADIR/icons/hicolor/256x256/apps
	icon.files += rsrc/icons/fuel.png

	INSTALLS += target desktop icon
	system(intl/convert.sh)
}


INCLUDEPATH += src

SOURCES += src/main.cpp\
	src/MainWindow.cpp \
	src/CommitDialog.cpp \
	src/FileActionDialog.cpp \
	src/SettingsDialog.cpp \
	src/Utils.cpp \
	src/FileTableView.cpp \
	src/CloneDialog.cpp \
	src/LoggedProcess.cpp \
	src/BrowserWidget.cpp \
	src/CustomWebView.cpp \
    src/Bridge.cpp

HEADERS  += src/MainWindow.h \
	src/CommitDialog.h \
	src/FileActionDialog.h \
	src/SettingsDialog.h \
	src/Utils.h \
	src/FileTableView.h \
	src/CloneDialog.h \
	src/LoggedProcess.h \
	src/BrowserWidget.h \
	src/CustomWebView.h \
    src/Bridge.h

FORMS    += ui/MainWindow.ui \
	ui/CommitDialog.ui \
	ui/FileActionDialog.ui \
	ui/SettingsDialog.ui \
	ui/CloneDialog.ui \
	ui/BrowserWidget.ui

RESOURCES += \
	rsrc/resources.qrc

CODECFORTR = UTF-8

TRANSLATIONS += \
	intl/en_US.ts \
	intl/el_GR.ts \
	intl/de_DE.ts \
	intl/es_ES.ts \
	intl/fr_FR.ts \
	intl/ru_RU.ts \
	intl/pt_PT.ts

