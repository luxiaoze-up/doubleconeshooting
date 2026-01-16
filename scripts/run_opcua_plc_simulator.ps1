Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$py = 'C:/Python313/python.exe'

Write-Host "Running OPC-UA PLC simulator..." -ForegroundColor Cyan
& $py (Join-Path $root 'tools\opcua_plc_simulator\vacuum_plc_simulator.py')
