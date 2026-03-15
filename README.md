tn5250
======

This is a fork of [tn5250](https://github.com/tn5250/tn5250) that adds a
headless automation mode (`--headless`) and provides a single static binary
release for Linux x86_64 -- no installation or dependencies required.

The original tn5250 is an implementation of the 5250 telnet protocol. It was
originally an implementation for Linux, but it has been reportedly compiled on
a number of other platforms. Contributed keyboard maps and termcap entries for
FreeBSD are in this tarball as well (see freebsd/README for more information).

Building from Git
-----------------

Skip to "Building and Installing" below if you got these sources from a
.tar.gz release file.

Certain files, such as the libtool support files and some shell scripts
which replace possibly missing commands on the target system are not in
git because we don't maintain them. They can be installed with the
following command:

```bash
./autogen.sh
```

This command requires current versions of the following packages, and the
generated files may not work properly.

```bash
automake
autoconf
libtool
```

You may receive an error the first time you run this script. If so, run
the script a second time to make sure you don't get an error (this is a bug
with automake).


Building and Installing
-----------------------

To build the emulator simply type the following:

```bash
./configure
make
make install
```

Additional (but decidedly generic) installation instructions are available
in the INSTALL file included in this distribution tarfile. Installation
instructions specific to your platform exist if you are using Linux or
FreeBSD -- they are in the linux/ and freebsd/ directories, respectively.
Please read these before telling us that the function keys don't work ;-)

The emulator uses the ncurses library for manipulating the console. Make
sure you have the ncurses development libraries installed before trying to
compile the source. There have been both reports of the standard BSD curses
working and not working, so you may have to install ncurses under *BSD.

X Windows
---------

To use the emulator under X Windows, use the provided `xt5250` shell script,
which sets up a standard `xterm` (it will *not* work with an `nxterm` or an
`rxvt` terminal).

There is one common problem which would cause `xt5250` to flash once on the
screen then disappear. If the termcap or terminfo entry for the "xterm-5250"
terminal type does not exist, `xterm` will exit immediately.

Windows
-------

To build on Windows, use [CMake](https://cmake.org/):

```ps1
New-Item -Path build -Type Directory
$cmake = (Join-Path -Path (Get-ItemProperty `
        -Path HKLM://HKEY_LOCAL_MACHINE\SOFTWARE\Kitware\CMake `
        -Name InstallDir).InstallDir `
        -ChildPath "bin/cmake.exe")
Start-Process -FilePath $cmake -Wait -NoNewWindow -ArgumentList "-S . -B .\build\"
Start-Process -FilePath $cmake -Wait -NoNewWindow -ArgumentList "--build build"
```

Headless Mode
-------------

The `tn5250 --headless` option provides a scriptable, UI-free interface for
automating AS/400 interactions. It reads commands from stdin (one per line) and
writes JSON responses to stdout, making it easy to drive from shell scripts,
Python, or any language that can manage a subprocess.

### Commands

| Command | Description |
|---------|-------------|
| `connect <host[:port]>` | Connect to an AS/400 system |
| `getscreen` | Dump the current screen as text |
| `getscreen json` | Dump screen with cursor position, dimensions, and indicators |
| `getfield <row> <col>` | Get field metadata and data at a screen position |
| `getfields` | Get all input fields on the current screen |
| `screendump` | Get all screen regions (input fields + output text) in order |
| `sendkey <keyname>` | Send a key (enter, f1-f24, tab, pgup, pgdn, etc.) |
| `type <text>` | Type text at the current cursor position |
| `movecursor <row> <col>` | Move the cursor to a screen position |
| `waitfor <text> [timeout]` | Block until text appears anywhere on screen (default 30s timeout) |
| `waitforat <row> <col> <text> [timeout]` | Block until text appears at a specific position (default 30s) |
| `waitready [timeout]` | Block until system is ready for input (default 30s timeout) |
| `quit` | Disconnect and exit |

### Response Format

All responses are single-line JSON:

```json
{"status":"ok","screen":"...","cursor":[5,20],"rows":24,"cols":80,"indicators":{...}}
{"status":"error","message":"not connected"}
```

### Example: Shell

```bash
printf 'connect myas400.example.com\nwaitfor User 30\ntype MYUSER\nsendkey tab\ntype MYPASS\nsendkey enter\nwaitfor Main Menu 30\ngetscreen\nquit\n' | tn5250 --headless
```

### Example: Python

```python
import subprocess, json

proc = subprocess.Popen(['tn5250', '--headless'],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

def send(cmd):
    proc.stdin.write(cmd + '\n')
    proc.stdin.flush()
    return json.loads(proc.stdout.readline())

send('connect myas400.example.com')
send('waitfor User 30')
send('type MYUSER')
send('sendkey tab')
send('type MYPASS')
send('sendkey enter')
resp = send('getscreen json')
print(resp['screen'])
send('quit')
```

### Key Names for `sendkey`

| Key Name | Description |
|----------|-------------|
| `enter` | Enter / Send |
| `tab` | Tab (next field) |
| `backtab` | Back Tab (previous field) |
| `f1` - `f24` | Function keys F1 through F24 |
| `up` | Cursor up |
| `down` | Cursor down |
| `left` | Cursor left |
| `right` | Cursor right |
| `pgup` / `pageup` | Page Up (Roll Down) |
| `pgdn` / `pagedown` | Page Down (Roll Up) |
| `home` | Home |
| `end` | End |
| `insert` | Insert toggle |
| `delete` | Delete character |
| `backspace` | Backspace |
| `newline` | New Line |
| `fieldexit` | Field Exit |
| `fieldplus` | Field Plus |
| `fieldminus` | Field Minus |
| `duplicate` | Duplicate |
| `reset` | Reset |
| `sysreq` | System Request |
| `clear` | Clear |
| `attention` | Attention |
| `help` | Help |
| `print` | Print |

Key names are case-insensitive.

### Building

Headless mode is built into the main `tn5250` binary -- no separate binary is
needed. It uses pthreads for the session thread, which is available on all POSIX
systems.

### Testing

An integration test script is included:

```bash
./headless/test_headless.sh
```

This runs both offline tests (argument validation, error handling) and live tests
against [pub400.com](https://pub400.com), a public IBM i server.

Other Information
-----------------

Other information is available on the web.

- [tn5250 Homepage](https://tn5250.github.io)
- [Issue tracker](https://github.com/tn5250/tn5250/issues)
- [Former home at SourceForge](http://sourceforge.net/projects/tn5250/)

Enjoy!
