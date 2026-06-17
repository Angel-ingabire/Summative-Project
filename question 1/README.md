Emergency Dispatch Incident Tracker (C implementation)

Overview

- A CLI incident tracker implemented in C using a doubly linked list.
- Stores incidents chronologically (oldest -> newest) with a maximum of 25 incidents.
- Live monitoring simulates incoming incidents while allowing navigation.

Commands

- `f` : View newer incident
- `b` : View older incident
- `l` : Enable live incident monitoring
- `s` : Stop live monitoring
- `d` : Delete all incidents
- `q` : Save session and quit

Build & Run (Windows)

1. Install a C compiler (MinGW-w64 or MSVC).
   2a. Using MinGW (Git Bash / MSYS):

```bash
gcc -o "question 1/tracker.exe" "question 1/tracker.c"
"question 1/tracker.exe"
```

2b. Using MSVC (Developer Command Prompt):

```bat
cl /nologo /W3 /MT "question 1\tracker.c" /Fe:question 1\tracker.exe
question 1\tracker.exe
```

Session

- The tracker saves the session to `question 1/session.json` on exit and loads it at startup.

Notes

- The C implementation uses Windows threading APIs and requires compilation on Windows.
- `session.json` is written as a simple JSON array; the loader uses a minimal parser tolerant of the program's own output.
