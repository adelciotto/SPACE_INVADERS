@echo off

set debug_dir=%~dp0..\debug
set release_dir=%~dp0..\release
set obj_dir=.\obj\

rd /s /q %debug_dir%
rd /s /q %release_dir%
