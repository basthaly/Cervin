@echo off
rem Cervin: what's new in Fritchii, Trudy and Eglantine since the last check.
rem Options: -Tout (every commit), -MarquerVu (save the current state as seen).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0cervin-sources.ps1" %*
if "%~1"=="" pause
