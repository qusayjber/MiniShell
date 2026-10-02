<div align="center">

# 🐚 MiniShell

### A modern, powerful Unix shell built from scratch in C

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Language: C11](https://img.shields.io/badge/Language-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20MSYS2-lightgrey.svg)]()
[![Build](https://img.shields.io/badge/Build-Passing-brightgreen.svg)]()
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)]()

*A shell with a real line editor, tab completion, persistent history, pipes,<br>
redirection, job control, and globbing — all written in pure C.*

</div>

---

<div align="center">

<a href="docs/screenshots/demo.png">
  <img src="docs/screenshots/demo.png" alt="MiniShell Demo" width="900">
</a>

<sub><i>MiniShell v2.1 running on Windows via MSYS2 — click to enlarge</i></sub>

</div>

---

## 📖 Table of Contents

- [🐚 MiniShell](#-minishell)
    - [A modern, powerful Unix shell built from scratch in C](#a-modern-powerful-unix-shell-built-from-scratch-in-c)
  - [📖 Table of Contents](#-table-of-contents)
  - [✨ Features](#-features)
    - [Core Shell Functionality](#core-shell-functionality)
    - [Modern Line Editor](#modern-line-editor)
    - [Job Control](#job-control)
    - [Persistent History](#persistent-history)
    - [Beautiful UI](#beautiful-ui)
  - [🎬 Demo](#-demo)
  - [📸 Screenshots](#-screenshots)
    - [Shell Preview](#shell-preview)
    - [More Screenshots](#more-screenshots)
  - [🧰 Requirements](#-requirements)
  - [🚀 Installation](#-installation)
    - [Linux / macOS](#linux--macos)
    - [Windows (MSYS2)](#windows-msys2)
  - [💻 Usage](#-usage)
    - [Options](#options)
    - [Basic Usage](#basic-usage)
  - [📚 Built-in Commands](#-built-in-commands)
  - [⌨️ Keyboard Shortcuts](#️-keyboard-shortcuts)
    - [Line Editing](#line-editing)
    - [History \& Control](#history--control)
  - [🏗 Architecture](#-architecture)
    - [Pipeline Stages](#pipeline-stages)
  - [📁 Project Structure](#-project-structure)
  - [🧪 Examples](#-examples)
    - [Pipes](#pipes)
    - [Redirection](#redirection)
    - [Globbing](#globbing)
    - [Environment](#environment)
    - [Background Jobs](#background-jobs)
    - [Sequential](#sequential)
  - [🔬 How It Works](#-how-it-works)
    - [Line Editor](#line-editor)
    - [Tab Completion](#tab-completion)
    - [Pipes \& Redirection](#pipes--redirection)
    - [Signal Handling](#signal-handling)
  - [🧪 Testing](#-testing)
    - [Valgrind (memory leaks)](#valgrind-memory-leaks)
    - [Sanitizers](#sanitizers)
  - [🤝 Contributing](#-contributing)
    - [Code Style](#code-style)
  - [🗺 Roadmap](#-roadmap)
  - [📜 License](#-license)
  - [👤 Author](#-author)
  - [🙏 Acknowledgments](#-acknowledgments)

---

## ✨ Features

### Core Shell Functionality
- ✅ **External command execution** via `fork()` + `execvp()`
- ✅ **Pipes** — chain unlimited commands with `|`
- ✅ **Redirection** — `>`, `>>`, `<`
- ✅ **Background jobs** — run commands with `&`
- ✅ **Sequential execution** — `cmd1 ; cmd2`
- ✅ **Environment expansion** — `$VAR`, `${VAR}`, `$?`, `~`
- ✅ **Globbing** — `*`, `?`, `[abc]`
- ✅ **Signal handling** — clean `Ctrl+C`, `Ctrl+D`, `Ctrl+Z`

### Modern Line Editor
- ✅ **Cursor movement** — arrows, `Home`, `End`
- ✅ **History navigation** — `↑` `↓` with persistent storage
- ✅ **Tab completion** — commands (from `$PATH`) + files & directories
- ✅ **Emacs-style bindings** — `Ctrl+A/E/U/K/W/L`
- ✅ **UTF-8 aware** — proper width calculation

### Job Control
- ✅ **Job tracking** — `jobs` builtin
- ✅ **Background process reaping** — no zombie processes
- ✅ **Status reporting** — exit codes, signal termination

### Persistent History
- ✅ Saved to `~/.minishell_history`
- ✅ Survives shell restarts
- ✅ Deduplication of consecutive commands
- ✅ 500-entry rolling buffer

### Beautiful UI
- ✅ Colorful two-line prompt
- ✅ ASCII banner
- ✅ Colored error/info messages
- ✅ Clean help output

---

## 🎬 Demo

```
┌─[qusai@minishell] ~/projects/myshell
└─❯ ls *.c *.h
shell.c  utils.h

┌─[qusai@minishell] ~/projects/myshell
└─❯ cat src/*.c | grep "main" | wc -l
3

┌─[qusai@minishell] ~/projects/myshell
└─❯ echo "hello world" > out.txt

┌─[qusai@minishell] ~/projects/myshell
└─❯ cat < out.txt
hello world

┌─[qusai@minishell] ~/projects/myshell
└─❯ sleep 30 &

┌─[qusai@minishell] ~/projects/myshell
└─❯ jobs
  [1]  Running  sleep

┌─[qusai@minishell] ~/projects/myshell
└─❯ type gcc
gcc is /usr/bin/gcc
```

---

## 📸 Screenshots

### Shell Preview

<div align="center">

<img src="docs/screenshots/demo.png" alt="MiniShell Banner and Prompt" width="900">

</div>

### More Screenshots

| Tab Completion | Help Output | Background Jobs |
|:---:|:---:|:---:|
| `docs/screenshots/tab.png` | `docs/screenshots/help.png` | `docs/screenshots/jobs.png` |

> **Want to add more?** Drop PNG files into `docs/screenshots/` and reference them here.

---

## 🧰 Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| **OS** | Linux, macOS, MSYS2 | Linux (Ubuntu 22.04+) |
| **Compiler** | GCC 10 / Clang 12 | GCC 13+ |
| **C Standard** | C11 | C11 |
| **Build Tool** | GNU Make | GNU Make |

**Windows users:** This project uses POSIX APIs (`fork`, `termios`, `unistd.h`).
Use **MSYS2 MSYS shell** (not MinGW/UCRT64), **WSL**, or **Cygwin**.

---

## 🚀 Installation

### Linux / macOS

```bash
# Clone
git clone https://github.com/qusaijaber/minishell.git
cd minishell

# Build
make

# Run
./minishell
```

**Manual build (without Make):**

```bash
gcc -Wall -Wextra -O2 -std=c11 -o minishell shell.c
./minishell
```

### Windows (MSYS2)

1. **Install MSYS2** from [msys2.org](https://www.msys2.org/)

2. **Open the "MSYS2 MSYS" terminal** (purple icon, **not** UCRT64)

3. **Install GCC and Make:**

```bash
pacman -S --noconfirm --needed gcc make
```

4. **Clone and build:**

```bash
git clone https://github.com/qusaijaber/minishell.git
cd minishell
make
./minishell.exe
```

5. **Run from PowerShell (optional):**

```powershell
# Copy the MSYS runtime DLL next to the binary
Copy-Item C:\msys64\usr\bin\msys-2.0.dll .

# Then run
.\minishell.exe
```

---

## 💻 Usage

```bash
./minishell [options]
```

### Options

| Flag | Description |
|------|-------------|
| `-h`, `--help` | Show help and exit |
| `-v`, `--version` | Show version and exit |
| `-c <cmd>` | Execute command and exit *(planned)* |

### Basic Usage

Once inside MiniShell:

```bash
# Run any system command
ls -la
cat file.txt
grep -r "pattern" .

# Use pipes
ps aux | grep bash | wc -l

# Redirect I/O
echo "hello" > file.txt
cat < file.txt >> log.txt

# Chain commands
cd ~/projects ; ls ; pwd

# Run in background
sleep 60 &
```

---

## 📚 Built-in Commands

| Command | Description |
|---------|-------------|
| `cd [dir]` | Change directory (`-` for previous) |
| `pwd` | Print working directory |
| `echo [-n] [args]` | Print arguments |
| `clear` | Clear screen and show banner |
| `history` | Show command history |
| `env` | List environment variables |
| `set VAR=VAL` | Set environment variable |
| `unset VAR` | Remove environment variable |
| `export VAR=VAL` | Export variable to child processes |
| `type <cmd>` | Show command type / location |
| `jobs` | List background jobs |
| `help` | Show help |
| `exit` \| `quit` | Exit shell |

---

## ⌨️ Keyboard Shortcuts

### Line Editing

| Key | Action |
|-----|--------|
| `←` / `→` | Move cursor |
| `Home` / `End` | Jump to start/end |
| `Backspace` | Delete char before cursor |
| `Delete` | Delete char at cursor |
| `Ctrl+A` | Move to beginning of line |
| `Ctrl+E` | Move to end of line |
| `Ctrl+U` | Delete from cursor to start |
| `Ctrl+K` | Delete from cursor to end |
| `Ctrl+W` | Delete previous word |

### History & Control

| Key | Action |
|-----|--------|
| `↑` / `↓` | Navigate history |
| `Tab` | Autocomplete |
| `Ctrl+L` | Clear screen |
| `Ctrl+C` | Cancel current line |
| `Ctrl+D` | Exit shell (empty line) |

---

## 🏗 Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         main()                              │
│                          │                                  │
│                          ▼                                  │
│              ┌───────────────────────┐                      │
│              │   print_banner()      │  (once)              │
│              └───────────┬───────────┘                      │
│                          ▼                                  │
│         ┌────────────────────────────────┐                  │
│         │        REPL Loop               │                  │
│         │  ┌──────────────────────────┐  │                  │
│         │  │  job_reap()              │  │                  │
│         │  │  build_prompt()          │  │                  │
│         │  │  read_line()  ◄──────────┼──┼── line editor    │
│         │  │     ├─ Tab completion    │  │                  │
│         │  │     ├─ History (↑/↓)     │  │                  │
│         │  │     └─ Ctrl shortcuts    │  │                  │
│         │  │  history_add()           │  │                  │
│         │  │  execute_line()          │  │                  │
│         │  └──────────────────────────┘  │                  │
│         └────────────────────────────────┘                  │
│                          │                                  │
│         ┌────────────────┼────────────────┐                 │
│         ▼                ▼                ▼                 │
│   ┌──────────┐   ┌──────────────┐   ┌──────────────┐        │
│   │ Built-in │   │ Pipe Chain   │   │ Redirection  │        │
│   │ Commands │   │ fork+exec    │   │  >  >>  <    │        │
│   └──────────┘   └──────────────┘   └──────────────┘        │
└─────────────────────────────────────────────────────────────┘
```

### Pipeline Stages

1. **Input** — `read_line()` reads a line with full editing support
2. **History** — the line is stored in the persistent history
3. **Sequential Split** — line is split by `;` (respecting quotes)
4. **Tokenization** — each segment becomes an argv array
5. **Env Expansion** — `$VAR`, `${VAR}`, `~`, `$?` are expanded
6. **Pipe Split** — tokens are split by `|`
7. **Parse** — each command extracts redirection & background flags
8. **Execute** — built-ins run inline; external commands are `fork`ed
9. **Wait** — parent waits for the pipeline (or reaps in background)

---

## 📁 Project Structure

```
minishell/
├── shell.c              # Main source (single file, ~1500 LOC)
├── Makefile             # Build system
├── README.md            # This file
├── LICENSE              # MIT License
├── .gitignore           # Git ignore rules
├── .editorconfig        # (optional) Editor config
├── .clang-format        # (optional) Code style
└── docs/
    └── screenshots/
        ├── demo.png
        ├── tab.png
        └── help.png
```

---

## 🧪 Examples

### Pipes

```bash
❯ cat /etc/passwd | cut -d: -f1 | sort | uniq
```

### Redirection

```bash
❯ ls -la > listing.txt
❯ echo "appended line" >> listing.txt
❯ wc -l < listing.txt
```

### Globbing

```bash
❯ ls *.c
❯ echo src/*/*.h
❯ rm file?.tmp
```

### Environment

```bash
❯ echo "Hello, $USER!"
❯ echo "Home: ${HOME}"
❯ echo "Last exit: $?"
```

### Background Jobs

```bash
❯ sleep 10 &
[bg] PID 12345
❯ jobs
  [1]  Running  sleep
```

### Sequential

```bash
❯ cd /tmp ; pwd ; ls ; cd -
```

---

## 🔬 How It Works

### Line Editor

The line editor puts the terminal into **raw mode** with `termios()`, then reads
one byte at a time. Special sequences (arrow keys, `Ctrl+<key>`) are intercepted
and mapped to buffer operations. After each change, `led_refresh()` redraws
the line using ANSI escapes.

### Tab Completion

On `Tab`, the current word is extracted. If we're at the command position
(first word or after `|`, `;`, `&`), we scan all directories in `$PATH` for
executables starting with the prefix. Otherwise, we scan the filesystem for
matching files. A common prefix is applied if multiple matches exist.

### Pipes & Redirection

For each command in the pipeline, a `pipe()` is created, `fork()` spawns a
child, and `dup2()` wires up `STDIN`/`STDOUT`. Redirection files are opened
with `open()` and similarly `dup2()`'d. The parent closes unused descriptors
and waits for all children.

### Signal Handling

`SIGINT` (`Ctrl+C`) is forwarded to the foreground process group if a command
is running, otherwise it prints a new prompt. In raw mode, the `ISIG` flag is
cleared so we handle `Ctrl+C` ourselves.

---

## 🧪 Testing

```bash
# Basic smoke test
make test

# Manual test scenarios
./minishell <<'EOF'
echo "hello" $USER
pwd
ls *.c | wc -l
exit
EOF
```

### Valgrind (memory leaks)

```bash
valgrind --leak-check=full --show-leak-kinds=all ./minishell
```

### Sanitizers

```bash
gcc -fsanitize=address,undefined -g -std=c11 -o minishell-debug shell.c
./minishell-debug
```

---

## 🤝 Contributing

Contributions are welcome! Please follow these steps:

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. **Commit** your changes (`git commit -m 'Add amazing feature'`)
4. **Push** to the branch (`git push origin feature/amazing-feature`)
5. **Open** a Pull Request

### Code Style

- C11, 4-space indentation
- Functions in `snake_case`
- Constants in `UPPER_CASE`
- Max line length: 80 chars (soft), 100 (hard)
- No compiler warnings with `-Wall -Wextra`

---

## 🗺 Roadmap

- [x] Line editor with history
- [x] Tab completion
- [x] Pipes & redirection
- [x] Background jobs
- [x] Globbing
- [x] Persistent history
- [ ] Aliases (`alias ll='ls -la'`)
- [ ] Command substitution `$(...)`
- [ ] Subshells `(...)`
- [ ] `&&` and `||` operators
- [ ] `Ctrl+R` reverse history search
- [ ] Config file (`~/.minishellrc`)
- [ ] Custom prompt (`PS1`)
- [ ] Script execution mode
- [ ] Multi-byte UTF-8 input

---

## 📜 License

This project is licensed under the **MIT License** — see the
[LICENSE](LICENSE) file for details.

---

## 👤 Author

**Qusai Jaber**

- GitHub: [@qusaijaber](https://github.com/qusaijaber)
- Email: *(add your email)*

---

## 🙏 Acknowledgments

- Inspired by [bash](https://www.gnu.org/software/bash/),
  [zsh](https://www.zsh.org/), and [fish](https://fishshell.com/)
- POSIX standard documentation
- [Writing a Shell in C](https://brennan.io/2015/01/16/write-a-shell-in-c/)
  by Stephen Brennan

---

<div align="center">

**⭐ If you find this project useful, please give it a star! ⭐**

Made with ❤️ and C

</div>