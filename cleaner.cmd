@echo off
PUSHD %~DP0
set CURRENTPATH = %cd%

:: Erase All Files created from FlashMem Simulator

:: FlashMemory Storage File
del /s /q "*storage*.bin"

:: FlashMemory Mapping Table File
del /s /q "*table*.bin"

:: Mapping Table output File
del /s /q "*table*.txt"

:: FlashMemory Storage Information File
del /s /q "*volume*.txt"

:: Index File for Round-Robin Based Spare Block Table
del /s /q "*rr_read_index*.txt"

:: Block meta-data Information Output File
del /s /q "*block_meta_output*.txt"

exit
