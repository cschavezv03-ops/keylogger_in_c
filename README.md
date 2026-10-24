# keylogger_in_c

Keylogger written in raw C for academic purposes. The client captures the keys
pressed on the local machine and streams them to a TCP server, which prints them
as the text that was actually typed (case, accents and symbols included).

> **Academic / lab use only.** Run this on your own machine or on a disposable
> virtual machine. Deploying it on a computer you do not own is illegal.

## Project structure

| File | Description |
|---|---|
| `src/server.c` | TCP server (port 8080). Receives and prints the typed text. |
| `src/client.c` | Keylogger client. Translates each keystroke and sends it to the server. |
| `src/service.c` | Windows service wrapper. Runs as a service and launches `client.exe` in the interactive user session. |
| `src/main.c` | Scratch file used while learning `read`/`write`. Not part of the keylogger. |

## Requirements

- Windows 10/11 (x64).
- MinGW-w64 GCC (the project was built and tested with GCC 16.1.0).
- Administrator rights only to install/remove the service.

## Build

Run from the repository root:

```powershell
gcc src\server.c  -o server.exe  -lws2_32
gcc src\client.c  -o client.exe  -lws2_32
gcc src\service.c -o service.exe -lwtsapi32 -luserenv -mwindows
```

`-lws2_32` links Winsock, `-lwtsapi32`/`-luserenv` provide the session APIs used
by the service, and `-mwindows` keeps the service binary without a console.

> If the linker reports `undefined reference to 'WTSQueryUserToken'`, the
> `-lwtsapi32` flag is missing. The library must come **after** the `.c` file.

In VS Code, `Ctrl+Shift+B` runs the `build: all` task (server, client and
service), which already includes the correct flags.

## Running standalone (development)

Start the server first, then the client:

```powershell
.\server.exe
```

```powershell
.\client.exe
```

The client hides its console (`FreeConsole`), so to stop it use Task Manager or
comment out that line while debugging. If the server is not running, the client
exits immediately because `connect` fails.

## Running the client as a Windows service

The service is registered as **`WinSystem Host Service`**. Because the name
contains spaces, it must be quoted in every `sc` command.

### Why a service wrapper

A service runs in **session 0**, which is isolated from the interactive desktop.
`GetAsyncKeyState` in a session 0 process cannot see what the user types, so the
keylogger itself cannot be the service. `service.exe` provides the Service
Control Manager interface and starts `client.exe` inside the logged-in user's
session with `WTSQueryUserToken` + `CreateProcessAsUser`.

```
Service Control Manager
        |
        v
service.exe  (session 0, LocalSystem)
        |
        v
client.exe   (session 1, logged-in user)  ->  server.exe (127.0.0.1:8080)
```

### 1. Build the binaries

See the [Build](#build) section. Three binaries are needed: `server.exe`,
`client.exe` and `service.exe`.

### 2. Stage the binaries on a local disk

A service does not see mapped network drives (`S:` in this project), so the
binaries must be copied to a local path. The service expects the client at
`C:\ProgramData\kl\client.exe` (see `CLIENT_PATH` in `src/service.c`).

```powershell
New-Item -ItemType Directory -Force "C:\ProgramData\kl"
Copy-Item client.exe, service.exe "C:\ProgramData\kl\"
```

### 3. Install the service

Open PowerShell **as Administrator**. Note the required space after every `=` in
`sc` arguments, and the quotes around the service name.

Create the service (auto-start at boot, running as `LocalSystem`):

```powershell
sc create "WinSystem Host Service" binPath= "C:\ProgramData\kl\service.exe" start= auto obj= LocalSystem
```

Optional but recommended, so Windows restarts it if it stops unexpectedly:

```powershell
sc failure "WinSystem Host Service" reset= 86400 actions= restart/5000/restart/5000/restart/5000
```

Optional description shown in `services.msc`:

```powershell
sc description "WinSystem Host Service" "Academic project - demo keylogger service"
```

If you prefer to start it manually instead of at boot, use `start= demand`
instead of `start= auto` in the `sc create` command.

> The name passed to `sc create` must match `SERVICE_NAME` in `src/service.c`
> exactly. If you change it, recompile `service.exe`, recreate the service, and
> update the commands in this section.

### 4. Start the service

```powershell
sc start "WinSystem Host Service"
```

### 5. Verify

```powershell
sc query "WinSystem Host Service"
```

`STATE` must be `4 RUNNING`. This is the evidence for the rubric:

```powershell
sc qc "WinSystem Host Service"
```

Check that the start type is `AUTO_START`, the binary path is
`C:\ProgramData\kl\service.exe` and the account is `LocalSystem`. You can also
confirm that the client is running in the user session:

```powershell
Get-Process client
```

```powershell
Get-CimInstance Win32_Service -Filter "Name='WinSystem Host Service'" |
    Select-Object Name, State, StartMode, StartName, PathName
```

Finally, start `server.exe` and type something: the keys must arrive.

### 6. Stop the service

```powershell
sc stop "WinSystem Host Service"
```

### 7. Remove the service

```powershell
sc stop "WinSystem Host Service"
sc delete "WinSystem Host Service"
Remove-Item -Recurse -Force "C:\ProgramData\kl"
```

## Quick reference

| Action | Command |
|---|---|
| Build server | `gcc src\server.c -o server.exe -lws2_32` |
| Build client | `gcc src\client.c -o client.exe -lws2_32` |
| Build service | `gcc src\service.c -o service.exe -lwtsapi32 -luserenv -mwindows` |
| Stage binaries | `New-Item -ItemType Directory -Force "C:\ProgramData\kl"; Copy-Item client.exe, service.exe "C:\ProgramData\kl\"` |
| Install (auto-start) | `sc create "WinSystem Host Service" binPath= "C:\ProgramData\kl\service.exe" start= auto obj= LocalSystem` |
| Install (on demand) | `sc create "WinSystem Host Service" binPath= "C:\ProgramData\kl\service.exe" start= demand obj= LocalSystem` |
| Restart policy | `sc failure "WinSystem Host Service" reset= 86400 actions= restart/5000/restart/5000/restart/5000` |
| Start | `sc start "WinSystem Host Service"` |
| Status | `sc query "WinSystem Host Service"` |
| Configuration | `sc qc "WinSystem Host Service"` |
| Check client process | `Get-Process client` |
| Stop | `sc stop "WinSystem Host Service"` |
| Uninstall | `sc stop "WinSystem Host Service"; sc delete "WinSystem Host Service"` |

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `1053: The service did not respond to the start or control request in a timely fashion` | The binary does not implement the SCM interface, or `SetServiceStatus(SERVICE_RUNNING)` is called too late (limit is 30 s). Use `service.exe`, not `client.exe`. |
| `1060: The specified service does not exist as an installed service` | Wrong service name (remember the quotes around `WinSystem Host Service`) or the shell is not elevated. |
| `1069: The service did not start due to a logon failure` | The service was not created with `obj= LocalSystem`. |
| `undefined reference to 'WTSQueryUserToken'` | Missing `-lwtsapi32` when building `service.exe`. |
| Service is `RUNNING` but no keys arrive | `client.exe` did not start in session 1. Check `WTSQueryUserToken`/`CreateProcessAsUser`, and that `si.lpDesktop` is `winsta0\default`. |
| Service starts at boot and then stops | No user session exists yet. `service.exe` retries every 3 s, so leave the retry loop in place; check again after logging in. |
| A window flashes while the server is down | `client.exe` is relaunched without `CREATE_NO_WINDOW`, so a console window is allocated and hidden. Rebuild `service.exe` and update `C:\ProgramData\kl\service.exe`. |
| `client.exe` relaunches every second | The server is not listening on port 8080. Start `server.exe` first. |
| `CreateProcessAsUser` fails with `1314` (privilege not held) | The service must run as `LocalSystem` so it can enable `SeTcbPrivilege`, `SeAssignPrimaryTokenPrivilege` and `SeIncreaseQuotaPrivilege`. |
| Antivirus removes the binary | Expected: keyloggers are flagged. Use a lab VM with a folder exclusion; do not disable security software on a real machine. |
| Port already in use | Another `server.exe` or `client.exe` is running. Close it or change `SERVER_PORT`/`CON_PORT`. |

## Notes

- The client sends **UTF-8 text** (lowercase unless Shift/CapsLock is pressed,
  plus accents and AltGr symbols). The server switches the console to UTF-8 with
  `SetConsoleOutputCP(CP_UTF8)`.
- The server prints a timestamp at the start of every captured line and writes
  one timestamped entry per keystroke to `keylog.txt` in its working directory.
  Special keys appear as `[ENTER]`, `[TAB]`, `[BACKSPACE]` and `[CLICK]`, so
  nothing is erased from the log when the user backspaces. A mouse click starts
  a new timestamped line, simulating that the user moved to another field.
- Only physical keys are captured. Paste (`Ctrl+V`) and IME input are not.
- `FreeConsole()` in `client.c` hides the window when it runs standalone. When
  launched by the service it has no effect.
- Uninstall the service when the demo is over, and keep the `sc delete` command
  in the report as cleanup evidence.
