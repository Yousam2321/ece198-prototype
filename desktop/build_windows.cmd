@echo off
setlocal
cd /d "%~dp0\.."
python -m pip install pyserial==3.5 pyinstaller==6.11.1 reportlab==4.4.9 pillow==11.0.0
if errorlevel 1 exit /b 1
python -m PyInstaller --noconfirm --clean --onefile --windowed --name ECE198_Results --paths tools --paths desktop --collect-data reportlab desktop\results_app.py
if errorlevel 1 exit /b 1
echo Executable created in dist\ECE198_Results.exe
pause
