@echo off
setlocal
title Cloud360 - Xenia Canary Experimental
set "BASE=%~dp0"
set "XEX=%BASE%cloud360_xenia.xex"
if not exist "%XEX%" set "XEX=%BASE%build\cloud360_xenia.xex"
if not exist "%XEX%" set "XEX=%BASE%..\build\cloud360_xenia.xex"
if not exist "%XEX%" (
  echo [ERRO] cloud360_xenia.xex nao encontrado.
  echo Copie o BAT ao lado do XEX, ou deixe-o na pasta scripts.
  pause
  exit /b 1
)
set "EMU=%BASE%xenia_canary.exe"
if not exist "%EMU%" if exist "%BASE%..\xenia_canary.exe" set "EMU=%BASE%..\xenia_canary.exe"
if not exist "%EMU%" (
  where xenia_canary.exe >nul 2>&1
  if errorlevel 1 (
    echo [ERRO] xenia_canary.exe nao encontrado.
    echo Coloque o Xenia Canary junto deste BAT ou no PATH.
    pause
    exit /b 1
  )
  set "EMU=xenia_canary.exe"
)
echo Executando o Cloud360 Xenia experimental...
"%EMU%" "%XEX%" --async_shader_compilation=false --readback_resolve=full
echo.
echo Se ocorrer erro, consulte cache\cloud360_xenia.log e xenia.log.
pause
endlocal
