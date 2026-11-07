@echo off
rem Start the development build of Voltaria with the Qt libraries of MSYS2.
set "PATH=C:\msys64\ucrt64\bin;%PATH%"
start "" "%~dp0..\build\qelectrotech.exe" %*
