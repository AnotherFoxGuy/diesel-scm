@echo off
setlocal EnableDelayedExpansion
set SCRIPTDIR=%CD%
set PRJDIR=%SCRIPTDIR%\..
set INTLDIR=%SCRIPTDIR%
set QTPATH=C:\Qt\QtSDK\Desktop\Qt\4.8.1\mingw

echo Converting localizations

del %PRJDIR\rsrc\intl\*
if not exist %PRJDIR\rsrc\intl\ mkdir %PRJDIR\rsrc\intl\

for %i in ( %INTLDIR%\*.ts ) do (
	set BASE=%~ni
	
	REM Convert all except the en_US which is 
	REM the original text in the code
	if not "!BASE!"=="en_US" (
		echo !BASE!
		%QTPATH%\bin\lrelease %i -qm %PRJDIR\rsrc\intl\!BASE!.qm
	)
)

endlocal