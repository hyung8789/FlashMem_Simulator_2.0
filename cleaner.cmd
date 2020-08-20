@echo off
PUSHD %~DP0
set CURRENTPATH=%cd%

if not exist "%CURRENTPATH%\플래시 메모리 시뮬레이터 2.0.exe" goto ERR
del storage.bin
del table.bin
del table.txt
del volume.txt
del rr_read_index.txt
del block_meta_output.txt
exit

:ERR
echo Must be in the same folder with FlashMem Simulator
pause
exit
