# TChat - Multi-User C Chat System

TChat is a high-performance, asynchronous chat application written in **Pure C** for Linux systems. It utilizes I/O multiplexing via `poll()` to handle multiple client connections in a single server thread, and POSIX threads on the client-side for a responsive TUI (Text User Interface).

## 🚀 Features
* **Asynchronous I/O:** Uses `poll()` for efficient socket management.
* **Multi-Client Support:** Handles multiple simultaneous connections.
* **Identity System:** Nickname registration and broadcast tagging.
* **Multithreaded Client:** Background listener thread allows receiving messages while typing.
* **TUI Polish:** ANSI escape codes for colored terminal output and prompt management.

---

## 🛠 Project Structure
* `src/`: Core logic for `server.c` and `client.c`.
* `include/`: Shared protocol headers and definitions.
* `bin/`: Compiled binary executables.
* `Makefile`: Automated build system with dependency tracking.

---

## 🏗 Installation & Building

### Prerequisites
* A Linux environment (Optimized for Arch Linux).
* `gcc` (GNU Compiler Collection).
* `make`.

### Build Instructions
To compile the entire project, navigate to the root directory and run:

```bash
make all
```
🖥 How to Use
1. Start the Server

Run the server first to begin listening for incoming connections:
Bash
```bash
./bin/server
```
2. Connect Clients

Open new terminal windows for each user and run:

```bash
./bin/client
```
Select 1 to connect.

Enter your desired nickname.

Start chatting! Use /exit to quit.

🧠 Technical Overview
The Protocol

TChat uses a fixed-size struct protocol defined in protocol.h. This ensures that both the server and client interpret the byte-stream identically.
I/O Strategy

Unlike basic chat apps that spawn a new thread for every user (which is heavy), the TChat Server uses a single-threaded poll() loop. This monitors all file descriptors (FDs) and only triggers logic when data is actually ready to be read.
TUI Management

The client uses carriage returns (\r) and ANSI escape codes to ensure that incoming messages don't break the user's current input line, providing a smooth CLI experience.

## ⚖️ License
This project is licensed under the **GNU General Public License v2.0**. 

> "Free software is a matter of liberty, not price." — Richard Stallman

You are free to use, study, share, and modify this software, as long as your modifications remain under the same license.
