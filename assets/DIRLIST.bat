@echo off
setlocal enabledelayedexpansion

:: Set the output file name
set "OUT_FILE=OUTPUT.txt"

:: Clear the output file if it already exists
if exist "%OUT_FILE%" del "%OUT_FILE%"

echo ================================================== >> "%OUT_FILE%"
echo   COMPLETE FILE AND FOLDER STRUCTURE
echo ================================================== >> "%OUT_FILE%"
echo. >> "%OUT_FILE%"

:: List every file and folder recursively
for /r %%I in (*) do (
    echo %%I >> "%OUT_FILE%"
)

echo. >> "%OUT_FILE%"
echo ================================================== >> "%OUT_FILE%"
echo   CODE FILE CONTENTS (SORTED BY PATH)
echo ================================================== >> "%OUT_FILE%"
echo. >> "%OUT_FILE%"

:: Find and sort all .c, .h, and .py files alphabetically by their full path
for /f "tokens=*" %%A in ('dir /b /s /a:-d *.c *.h *.py 2^>nul ^| sort') do (
    echo -------------------------------------------------- >> "%OUT_FILE%"
    echo FILE: %%A >> "%OUT_FILE%"
    echo -------------------------------------------------- >> "%OUT_FILE%"
    echo. >> "%OUT_FILE%"
    
    :: Print the contents of the file
    type "%%A" >> "%OUT_FILE%"
    
    echo. >> "%OUT_FILE%"
    echo. >> "%OUT_FILE%"
)

echo Done! Output saved to %OUT_FILE%
pause