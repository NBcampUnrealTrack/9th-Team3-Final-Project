@echo off
rem 번역 시트 도구 실행기. 사용법: loc.bat gather ^| export ^| check ^| import  (자세한 내용은 README.md)
rem 엔진에 들어 있는 Python을 쓴다. 엔진을 다른 곳에 설치했다면 UE_ROOT 환경 변수에 UE_5.8 폴더를 넣는다.
chcp 65001 >nul
if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PY=%UE_ROOT%\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
if not exist "%PY%" (
    echo 엔진 Python을 찾지 못했습니다: %PY%
    echo UE_ROOT 환경 변수에 엔진 폴더를 넣어 주세요.
    exit /b 1
)
set PYTHONIOENCODING=utf-8
"%PY%" -I "%~dp0loc_sheet.py" %*
