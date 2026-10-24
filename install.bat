@echo off
echo Requesting administrative privileges...
:: Check for admin rights
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Administrator rights confirmed.
) else (
    echo Requesting privileges...
    powershell -Command "Start-Process '%~0' -Verb RunAs"
    exit /b
)

echo.
echo Copying MOSAIC.vst3 to your VST3 folder...
xcopy "%~dp0build_check\MOSAIC_artefacts\Release\VST3\MOSAIC.vst3" "C:\Program Files\Common Files\VST3\MOSAIC.vst3\" /E /I /H /Y

echo.
echo Done! You can now load MOSAIC in your DAW.
pause
