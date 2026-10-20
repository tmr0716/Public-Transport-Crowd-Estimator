@echo off
echo ==========================================
echo Building CrowdWise C++17 Engine...
echo ==========================================

if not exist build mkdir build
if not exist data mkdir data

:: Compile C++ executable using g++ (MinGW)
g++ -std=c++17 src/main.cpp src/crowd_engine.cpp -o build/crowd_engine.exe

if %errorlevel% neq 0 (
    echo [ERROR] C++ Compilation failed! Please ensure g++ is installed and added to PATH.
    pause
    exit /b %errorlevel%
)

echo [SUCCESS] C++ Engine compiled successfully to build/crowd_engine.exe
echo.
echo ==========================================
echo Setting up Python Environment & Flask...
echo ==========================================

python -m pip install -r requirements.txt

echo.
echo ==========================================
echo Starting CrowdWise Web Application...
echo Access in browser at: http://127.0.0.1:5000
echo ==========================================

python app.py
pause
