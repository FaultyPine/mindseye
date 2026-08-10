@echo off



set root=%~dp0
:: remove trailing backslash
set root=%root:~0,-1%
:: working dir should be project root (same dir as this script)
pushd %root%


"build/driver.exe"
