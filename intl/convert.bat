@echo off
setlocal EnableDelayedExpansion
set SCRIPTDIR=%CD%
set PRJDIR=%SCRIPTDIR%\..
set QTPATH=C:\Qt\5.4\mingw491_32

echo Converting localizations
del /q %PRJDIR%\rsrc\intl\*
if not exist %PRJDIR%\rsrc\intl\ mkdir %PRJDIR%\rsrc\intl\

REM Convert all except the en_US which is the original text in the code

%QTPATH%\bin\lrelease de_DE.ts -qm ..\rsrc\intl\de_DE.qm
%QTPATH%\bin\lrelease el_GR.ts -qm ..\rsrc\intl\el_GR.qm
%QTPATH%\bin\lrelease es_ES.ts -qm ..\rsrc\intl\es_ES.qm
%QTPATH%\bin\lrelease fr_FR.ts -qm ..\rsrc\intl\fr_FR.qm
%QTPATH%\bin\lrelease ru_RU.ts -qm ..\rsrc\intl\ru_RU.qm
%QTPATH%\bin\lrelease ru_RU.ts -qm ..\rsrc\intl\pt_PT.qm

endlocal
