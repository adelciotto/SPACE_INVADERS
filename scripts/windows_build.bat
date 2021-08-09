@echo off

set arg1=%1
set debug_mode=0
if "%arg1%" == "debug" set debug_mode=1

set build_dir=%~dp0..\release
set compile_flags=-diagnostics:column -nologo /Fe:%target_exe% /WL /GR /EHa /Oi /W4 /DDIMGUI_IMPL_OPENGL_LOADER_GLAD
if %debug_mode% == 1 (
  set build_dir=%~dp0..\debug
  set compile_flags=%compile_flags% -Zi /DEBUG:FULL
) else (
  set compile_flags=%compile_flags% /O2
)

rem 

if not exist %build_dir% mkdir %build_dir%

pushd %build_dir%

set obj_dir=.\obj\
set sdl2_dir=..\external\windows\SDL2-2.0.14
set sdl2mixer_dir=..\external\windows\SDL2_mixer-2.0.4

set target_exe=space_invaders.exe
set src_files=..\code\lib\glad\src\glad.cpp^
              ..\code\lib\imgui\imgui.cpp^
              ..\code\lib\imgui\imgui_draw.cpp^
              ..\code\lib\imgui\imgui_widgets.cpp^
              ..\code\lib\imgui\imgui_tables.cpp^
              ..\code\lib\imgui\imgui_impl_sdl.cpp^
              ..\code\lib\imgui\imgui_impl_opengl3.cpp^
              ..\code\lib\adc_8080_cpu.cpp^
              ..\code\opengl_spinvaders_shaders.cpp^
              ..\code\opengl_spinvaders_renderer.cpp^
              ..\code\spinvaders_effects.cpp^
              ..\code\spinvaders_machine.cpp^
              ..\code\spinvaders_imgui.cpp^
              ..\code\spinvaders.cpp^
              ..\code\sdl2_spinvaders_sound.cpp^
              ..\code\sdl2_spinvaders.cpp

set include_flags=/I %sdl2_dir%\include\ /I %sdl2mixer_dir%\include\ /I ..\code\lib\glad\include
set linker_flags=%sdl2_dir%\lib\x64\SDL2main.lib %sdl2_dir%\lib\x64\SDL2.lib %sdl2mixer_dir%\lib\x64\SDL2_mixer.lib opengl32.lib shell32.lib

echo.
echo Compiling space_invaders...
echo.

if not exist %obj_dir% mkdir %obj_dir%

cl %compile_flags% -MD %include_flags% /Fo%obj_dir% %src_files% /link /SUBSYSTEM:WINDOWS %linker_flags%

if %errorlevel% neq 0 (
  echo.
  echo space_invaders compilation failed!
  echo.

  popd
  exit /b %errorlevel%
)

echo.
echo space_invaders compilation successful!
echo.

echo.
echo Copying required data and dll's...
echo.

xcopy /y %sdl2_dir%\lib\x64\SDL2.dll
xcopy /y %sdl2mixer_dir%\lib\x64\SDL2_mixer.dll
xcopy /y /I ..\data data

echo.
echo data and dll's copied successfully!
echo.

popd
