@echo off
cd /d "C:\Program Files\usbipd-win"

echo === Listing USB devices ===
usbipd.exe list
echo.

REM 自动找 CP210x / ESP32 设备的 busid
for /f "tokens=1" %%i in ('usbipd.exe list ^| findstr /i "CP210\|Silicon\|Espressif\|USB JTAG"') do (
    set BUSID=%%i
    goto :found
)
echo [ERROR] ESP32-S3 device not found!
pause
exit /b 1

:found
echo === Binding %BUSID% ===
usbipd.exe bind --busid %BUSID% --force
echo.

echo === Attaching to WSL ===
usbipd.exe attach --busid %BUSID% --wsl
echo.

echo === Done ===
pause