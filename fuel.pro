#-------------------------------------------------
# Fuel
#-------------------------------------------------

lessThan(QT_MAJOR_VERSION, 5) {
	message("Unsupported Qt version $${QT_VERSION}.")
	error("Fuel requires Qt 5.4.0 or greater")
}

# Fuel requires at least Qt 5.4 due to QtWebEngine
equals(QT_MAJOR_VERSION, 5):lessThan(QT_MINOR_VERSION, 4) {
	message("Unsupported Qt version $${QT_VERSION}.")
	error("Fuel requires Qt 5.4.0 or greater")
}

QT = core gui widgets webengine webenginewidgets
QT-= quick multimediawidgets opengl printsupport qml multimedia positioning sensors


TARGET = Fuel
TEMPLATE = app

win32 {
	RC_FILE = rsrc/fuel.rc
	LIBS += -luser32 -lshell32 -luuid

	# Prevent batch file being called multiple times
	!build_pass:system(intl\convert.bat)
}

macx {
	ICON = rsrc/icons/fuel.icns
}

# FreeBSD needs explicit paths to Qt install
unix:freebsd {
	INCLUDEPATH += /usr/local/include
	LIBS += -L/usr/local/lib
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
	src/AppSettings.cpp \
	src/RemoteDialog.cpp \
    src/AboutDialog.cpp

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
	src/AppSettings.h \
	src/RemoteDialog.h \
	src/AboutDialog.h \
	src/WorkspaceCommon.h

FORMS    += ui/MainWindow.ui \
	ui/CommitDialog.ui \
	ui/FileActionDialog.ui \
	ui/SettingsDialog.ui \
	ui/FslSettingsDialog.ui \
	ui/CloneDialog.ui \
	ui/BrowserWidget.ui \
	ui/RevisionDialog.ui \
	ui/RemoteDialog.ui \
	ui/AboutDialog.ui

RESOURCES += \
	rsrc/resources.qrc

# QtKeychain
SOURCES += ext/qtkeychain/keychain.cpp

HEADERS += ext/qtkeychain/keychain.h \
		ext/qtkeychain/keychain_p.h \
		ext/qtkeychain/qkeychain_export.h

DEFINES += QKEYCHAIN_STATICLIB

unix:!macx {
	QT += dbus

	SOURCES += ext/qtkeychain/keychain_unix.cpp \
		ext/qtkeychain/gnomekeyring.cpp

	HEADERS += ext/qtkeychain/gnomekeyring_p.h
	DBUS_INTERFACES += ext/qtkeychain/org.kde.KWallet.xml
}

macx {
	SOURCES += ext/qtkeychain/keychain_mac.cpp
	LIBS += -framework CoreFoundation -framework Security
}

win32 {
	SOURCES += ext/qtkeychain/keychain_win.cpp
	LIBS += -lCrypt32
}




CODECFORTR = UTF-8

TRANSLATIONS += \
	intl/en_US.ts \
	intl/el_GR.ts \
	intl/de_DE.ts \
	intl/es_ES.ts \
	intl/fr_FR.ts \
	intl/ru_RU.ts \
	intl/pt_PT.ts \
	intl/it_IT.ts \
	intl/nl_NL.ts \
	intl/ko_KR.ts \
