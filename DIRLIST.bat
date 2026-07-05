@echo off
setlocal enabledelayedexpansion

:: Set the output file name
set "OUT_FILE=OUTPUT.txt"

:: Clear the output file if it already exists
if exist "%OUT_FILE%" del "%OUT_FILE%"

echo ================================================== >> "%OUT_FILE%"
echo   COMPLETE FILE AND FOLDER STRUCTURE >> "%OUT_FILE%"
echo ================================================== >> "%OUT_FILE%"
echo. >> "%OUT_FILE%"

echo Scanning full file structure...
echo.

:: List every file and folder recursively
for /r %%I in (*) do (
    :: Print to console
    echo %%I
    :: Print to output file
    echo %%I >> "%OUT_FILE%"
)

echo. >> "%OUT_FILE%"
echo ================================================== >> "%OUT_FILE%"
echo   CODE FILE CONTENTS (SORTED BY PATH) >> "%OUT_FILE%"
echo ================================================== >> "%OUT_FILE%"
echo. >> "%OUT_FILE%"

echo.
echo Extracting file contents...
echo.

:: Find and sort all target files alphabetically by their full path
for /f "tokens=*" %%A in ('dir /b /s /a:-d *.c *.h *.py *.fam *.scons SConstruct 2^>nul ^| sort') do (
    :: Print the file currently being extracted to the console
    echo Adding contents of: %%A
    
    echo -------------------------------------------------- >> "%OUT_FILE%"
    echo FILE: %%A >> "%OUT_FILE%"
    echo -------------------------------------------------- >> "%OUT_FILE%"
    echo. >> "%OUT_FILE%"
    
    :: Print the contents of the file strictly to the output file ONLY
    type "%%A" >> "%OUT_FILE%"
    
    echo. >> "%OUT_FILE%"
    echo. >> "%OUT_FILE%"
)

echo.
echo Done! Output saved to %OUT_FILE%
pause