@echo off
setlocal EnableDelayedExpansion
set SCRIPTDIR=%~dp0%
set PRJDIR=%SCRIPTDIR%..
set QTPATH=C:\Qt\5.5\mingw492_32

if NOT "%QTDIR%"=="" set QTPATH=%QTDIR%

echo Using QT at %QTPATH%
echo Converting localizations
del /q %PRJDIR%\rsrc\intl\*
if not exist %PRJDIR%\rsrc\intl\ mkdir %PRJDIR%\rsrc\intl\

REM Convert all except the en_US which is the original text in the code

for %%i in (de_DE el_GR es_ES fr_FR ru_RU pt_PT it_IT nl_NL ko_KR) do (
	%QTPATH%\bin\lrelease %PRJDIR%\intl\%%i.ts -qm %PRJDIR%\rsrc\intl\%%i.qm
)

endlocal
