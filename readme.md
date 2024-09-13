Implement a failsafe mode that starts a basic web server if the main one fails.
Add a watchdog timer to auto-reset the device if it hangs.
Include remote reset functionality via MQTT or a specific web endpoint.
Implement a fallback AP mode if WiFi connection fails.

powershell.exe -Command "& { & 'C:\Program Files\AutoHotkey\controlmymonitor\ControlMyMonitor.exe' /SetValue '\\.\DISPLAY3\Monitor0' 60 17 /SetValue '\\.\DISPLAY1\Monitor0' 60 18 /SetValue '\\.\DISPLAY2\Monitor0' 60 17 }"

CREATE FOLDER STRUCTURE
Get-ChildItem -Recurse | Where-Object { $_.PSIsContainer } | Select-Object FullName | ForEach-Object { $_.FullName.Replace($PWD.Path, "").TrimStart("\") } > structure.txt