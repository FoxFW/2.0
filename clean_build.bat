@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul




:MENU
cls
echo ===================================================
echo  MASTER COMPILATION OPERATOR AUTOMATION
echo ===================================================
echo.
echo  [F] Clean Workspace + Flash Live USB [flash_usb_full]
echo  [U] Clean Workspace + Generate TGZ ^& DFU [updater_package]
echo  [C] Run fbt -c
echo  [K] Delete Build, Dist, ^& scons.dblite
echo  [E] Delete Build, Dist, ^& scons.dblite + run fbt -c
echo  [N] Native Fast Compilation Pass Only [fbt]
echo.
echo  [M] Min-Package (DFU ONLY) [fbt updater_minpackage]
echo  [A] Exit Fox Master Compilation window to CMD
echo  [D] Exit Fox Master Compilation window and Close Window
echo.
echo ===================================================
echo.

:: Capture character keys instantly without pressing Enter
choice /c FUCKENMAD /n /m "Select your compilation route: "

if errorlevel 9 goto CMD_EXIT
if errorlevel 8 goto CMD_GOTO
if errorlevel 7 goto MINPKG
if errorlevel 6 goto JUSTFBT
if errorlevel 5 goto CLEAN_FBT
if errorlevel 4 goto JUST_CLEAN
if errorlevel 3 goto FBT_C
if errorlevel 2 goto UPDPKG
if errorlevel 1 goto LVEUSB

:LVEUSB
cls
echo ===================================================
echo 🚀 CLEANING WORKSPACE AND LAUNCHING LIVE USB FLASH...
echo ===================================================
if exist build rmdir /s /q build
if exist dist rmdir /s /q dist
if exist .sconsign.dblite del /f /q .sconsign.dblite
call fbt -c

:LOOP_LVEUSB
echo Running live USB flash...

:: Stream fbt output live, set flag on Permission/Denied, restart if Error seen after flag is set
powershell -Command "$flag=$false; $restart=$false; cmd.exe /c fbt flash_usb_full COMPACT=1 DEBUG=0 FORCE=1 2>&1 | ForEach-Object { Write-Host $_; if ($_ -match '(?i)permission|(?i)denied') { $flag=$true }; if ($_ -match '(?i)error' -and $flag) { $restart=$true } }; if ($restart) { exit 0 } else { exit 1 }"
if %errorlevel%==0 (
 echo.
 echo Permission/Denied + Error detected! Retrying command...
 echo.
 goto LOOP_LVEUSB
)
echo.
echo Press any key to return to Master Menu...
pause >nul
goto MENU

:UPDPKG
cls
echo ===================================================
echo 🚀 CLEANING WORKSPACE AND GENERATING UPDATER PACKAGE...
echo ===================================================
if exist build rmdir /s /q build
if exist dist rmdir /s /q dist
if exist .sconsign.dblite del /f /q .sconsign.dblite
call fbt -c

:LOOP_UPDPKG
echo Running updater package...

:: Stream fbt output live, set flag on Permission/Denied, restart if Error seen after flag is set
powershell -Command "$flag=$false; $restart=$false; cmd.exe /c fbt updater_package COMPACT=1 DEBUG=0 FORCE=1 2>&1 | ForEach-Object { Write-Host $_; if ($_ -match '(?i)permission|(?i)denied') { $flag=$true }; if ($_ -match '(?i)error' -and $flag) { $restart=$true } }; if ($restart) { exit 0 } else { exit 1 }"
if %errorlevel%==0 (
 echo.
 echo Permission/Denied + Error detected! Retrying command...
 echo.
 goto LOOP_UPDPKG
)
echo.
echo Press any key to return to Master Menu...
pause >nul
goto MENU

:FBT_C
cls
echo ===================================================
echo 🚀 RUNNING FBT CLEAN (-c)...
echo ===================================================
call fbt -c
echo.
echo Press any key to return to Master Menu...
pause >nul
goto MENU

:JUST_CLEAN
cls
echo ===================================================
echo 🚀 DELETING BUILD, DIST, AND SCONS.DBLITE...
echo ===================================================
if exist build rmdir /s /q build
if exist dist rmdir /s /q dist
if exist .sconsign.dblite del /f /q .sconsign.dblite
echo Done.
echo.
echo Press any key to return to Master Menu...
pause >nul
goto MENU

:CLEAN_FBT
cls
echo ===================================================
echo 🚀 DELETING FILES AND RUNNING FBT CLEAN...
echo ===================================================
if exist build rmdir /s /q build
if exist dist rmdir /s /q dist
if exist .sconsign.dblite del /f /q .sconsign.dblite
call fbt -c
echo.
echo Press any key to return to Master Menu...
pause >nul
goto MENU

:JUSTFBT
cls
echo ===================================================
echo 🚀 LAUNCHING NATIVE FAST COMPILATION PASS ONLY...
echo ===================================================
call fbt
echo.
echo Press any key to return to Master Menu...
pause >nul
goto MENU

:MINPKG
cls
echo ===================================================
echo 🚀 RUNNING MIN-PACKAGE (DFU ONLY)...
echo ===================================================
call fbt updater_minpackage
echo.
echo Press any key to return to Master Menu...
pause >nul
goto MENU

:CMD_GOTO
cls
echo 📄 Opening command prompt in this folder...
echo.
start cmd /k
exit

:CMD_EXIT
cls
echo 📄 Closing operator console window...
exit