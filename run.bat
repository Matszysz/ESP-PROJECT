@echo off
echo ========================================
echo ESP32 Build and Upload Script
echo ========================================
echo.

echo Step 1: Building project...
pio run
if %errorlevel% neq 0 (
    echo Build failed! Check errors above.
    pause
    exit /b 1
)

echo.
echo Step 2: Uploading to ESP32...
pio run --target upload
if %errorlevel% neq 0 (
    echo Upload failed! Check COM port and connection.
    pause
    exit /b 1
)

echo.
echo ========================================
echo SUCCESS! Code uploaded to ESP32
echo ========================================
echo.
echo To view serial output, run: pio device monitor
pause