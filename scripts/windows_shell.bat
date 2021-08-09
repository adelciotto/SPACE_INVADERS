@echo off

set scripts_dir=%~dp0

cd /d %scripts_dir%..

:: uncomment the line below line to debug the vcvars
:: set VSCMD_DEBUG=1
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"

set path=%scripts_dir%;%path%
