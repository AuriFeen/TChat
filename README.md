# TChat

**Pure C. No dependencies. No Docker. No npm install. Just `make` and talk.**

TChat is a self-hosted terminal chat server and client built from scratch in C. It runs on a $5 Pi, a decade-old laptop, or whatever Debian box you have collecting dust in the closet. Your computer *is* the server. Your friends connect directly to your IP. That's it.

---

## What It Actually Does

- **Host a chat room** on any Linux machine.
- **Connect from anywhere** on the same network instantly.
- **Connect from the internet** with one port forward (or UPnP if your router isn't garbage).
- **Human-readable aliases** — no more memorizing `46.190.121.120:51343`. Your friend types `rustbucket` and the config resolves it.
- **Private messages**, user lists, timestamps, and a settings dashboard inside the terminal.
- **STUN discovery** — the server tells you your public IP automatically. No `whatismyip.com` tab required.

---

## Features

| Feature | What It Means |
|---|---|
| `poll()`-based server | One thread, many clients. No pthread explosion on the server side. |
| POSIX threaded client | Background listener thread so you can type while messages arrive. |
| Raw TCP sockets | No HTTP bloat, no WebSocket handshakes, no TLS certificate hell. |
| STUN integration | Asks Google/Cloudflare "what's my public IP?" so you know what to give your friend. |
| UPnP auto-mapping | Server politely asks your router to open port 7777. Works on most home networks. |
| `.conf` aliases | Map `rustbucket = 203.0.113.45:7777` in `tchat_client.conf`. Type `rustbucket` at the prompt. |
| Terminal UI | ANSI colors, non-blocking input, settings menu navigable with arrow keys. |
| Zero dependencies | `gcc`, `make`, `libc`. That's the entire dependency tree. Binary is under 100KB. |
| No Iroh, no Tailscale, no VPS | We are not shipping a 40MB statically-linked Go binary to do what `socket()` already does. |

---

## Build

```bash
git clone https://github.com/AuriFeen/TChat.git
cd TChat
make
```

Requires: `gcc`, `make`, standard Linux headers. Nothing else.

---

## Quick Start

### 1. Start the Server

```bash
./bin/server
```

Output:
```
========================================================
 TChat Server
 Alias: rustbucket
 Public Endpoint: 46.190.121.120:7777
 Give your friend:  46.190.121.120:7777
 Listening on port: 7777
========================================================
```

If you see a `[UPnP] Mapped...` line, your router opened the port automatically. If not, forward TCP port 7777 to your server's local IP and you're done.

Create `tchat_server.conf` to customize:

```ini
[server]
alias = rustbucket
port = 7777
stun_host = stun.l.google.com
stun_port = 19302
upnp = true
```

### 2. Connect a Client

On the same machine, another machine on your LAN, or across the internet:

```bash
./bin/client
```

```
--- Welcome to TChat Global Overlay Mesh Framework ---
Enter target server address or alias: 192.168.1.147:7777
Connecting to 192.168.1.147:7777 ...
Enter system registration identity pseudonym token: auri
[22:33:04] [System Header] auri entered the secure channel.
> hello
[22:33:12] [auri]: hello
```

Create `tchat_client.conf` to save aliases:

```ini
[peers]
rustbucket = 46.190.121.120:7777
homelab    = 192.168.1.147:7777
```

Then just type `rustbucket` or `homelab` at the prompt.

---

## Client Commands

| Command | Action |
|---|---|
| `/exit` | Disconnect and quit. |
| `/settings` | Open the TUI configuration dashboard (timestamps, notification pings). |
| `/users` | Request the active user list from the server. |
| `/name <newname>` | Change your nickname. |
| `/msg <user> <text>` | Send a private message. |

---

## Architecture

```
┌─────────────┐         TCP 7777          ┌─────────────┐
│   Server    │ <-----------------------> │   Client    │
│  (poll())   │    TWireHeader + Payload  │  (pthread)  │
└─────────────┘                           └─────────────┘
      │
      └── STUN query to discover public IP
      └── UPnP request to auto-open router port
```

- **Protocol**: Fixed-size binary header (`TWireHeader`) + payload (`TChatPayload`). CRC32 checksum. No JSON, no XML, no protobuf.
- **Server**: Single-threaded `poll()` loop. Accepts connections, buffers partial packets in per-client ring buffers, broadcasts chat messages.
- **Client**: Main thread handles raw terminal input. Background thread blocks on `recv()` and prints formatted messages without destroying your input line.

---

## The Honest Limitations

| Scenario | Result |
|---|---|
| Same LAN / WiFi | Works instantly. Zero config. |
| UPnP enabled router | Works over internet automatically. |
| Manual port forward | Works over internet with 30 seconds of router admin panel pain. |
| CGNAT / Symmetric NAT (mobile hotspot, university, Starlink) | **Will not work without a relay/VPS.** No C program can defeat math. |
| Windows | Untested. WSL probably works. Cygwin is your own fault. |

---

## Why Not Just Use Discord / IRC / Matrix?

Because this is yours. No terms of service. no data mining, no electron app eating 400MB of RAM to display text. You run the binary, you own the conversation, you pull the plug when you're done.

It's the same reason Minecraft multiplayer blew up in 2010: anyone could run a server, give their friends an IP, and play. TChat is that, but for talking shit in a terminal.
