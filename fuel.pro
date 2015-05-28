#-------------------------------------------------
# Fuel
#-------------------------------------------------

QT    = core gui webkit

contains(QT_VERSION, ^5\\..*) {
	QT += widgets webkitwidgets
	QT -= quick multimediawidgets opengl printsupport qml multimedia positioning sensors
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
	src/FslSettingsDialog.cpp \
	src/CloneDialog.cpp \
	src/RevisionDialog.cpp \
	src/Utils.cpp \
	src/FileTableView.cpp \
	src/LoggedProcess.cpp \
	src/BrowserWidget.cpp \
	src/CustomWebView.cpp \
	src/Fossil.cpp \
	src/Workspace.cpp \
	src/SearchBox.cpp \
	src/Settings.cpp \
	src/RemoteDialog.cpp

HEADERS  += src/MainWindow.h \
	src/CommitDialog.h \
	src/FileActionDialog.h \
	src/SettingsDialog.h \
	src/FslSettingsDialog.h \
	src/CloneDialog.h \
	src/RevisionDialog.h \
	src/Utils.h \
	src/FileTableView.h \
	src/LoggedProcess.h \
	src/BrowserWidget.h \
	src/CustomWebView.h \
	src/Fossil.h \
	src/Workspace.h \
	src/SearchBox.h \
	src/Settings.h \
	src/RemoteDialog.h

FORMS    += ui/MainWindow.ui \
	ui/CommitDialog.ui \
	ui/FileActionDialog.ui \
	ui/SettingsDialog.ui \
	ui/FslSettingsDialog.ui \
	ui/CloneDialog.ui \
	ui/BrowserWidget.ui \
	ui/RevisionDialog.ui \
	ui/RemoteDialog.ui

RESOURCES += \
	rsrc/resources.qrc

# QtKeychain
SOURCES += ext/qtkeychain/keychain.cpp

HEADERS += ext/qtkeychain/keychain.h \
		ext/qtkeychain/keychain_p.h \
		ext/qtkeychain/qkeychain_export.h

unix:!macx {
	QT += dbus

	SOURCES += ext/qtkeychain/keychain_unix.cpp \
		ext/qtkeychain/gnomekeyring.cpp

	HEADERS += ext/qtkeychain/gnomekeyring_p.h
	DBUS_INTERFACES += ext/qtkeychain/org.kde.KWallet.xml
}

macx {
	SOURCES += ext/qtkeychain/keychain_mac.cpp
}

win32 {
	SOURCES += ext/qtkeychain/keychain_win.cpp
}




CODECFORTR = UTF-8

TRANSLATIONS += \
	intl/en_US.ts \
	intl/el_GR.ts \
	intl/de_DE.ts \
	intl/es_ES.ts \
	intl/fr_FR.ts \
	intl/ru_RU.ts \
	intl/pt_PT.ts

