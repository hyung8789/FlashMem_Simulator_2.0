@echo off
PUSHD %~DP0
set CURRENTPATH=%cd%

del storage.bin
del table.bin
del table.txt
del volume.txt
del rr_read_index.txt
del block_meta_output.txt
