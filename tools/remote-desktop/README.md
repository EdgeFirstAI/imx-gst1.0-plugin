
# remote_desktop.py

`remote_desktop.py` is a versatile tool that can operate in **server mode** or **client mode** to enable screen sharing, video streaming, and optional remote control. It supports multiple NXP i.MX boards, including **i.MX95, i.MX8MP, i.MX8QM, i.MX8QXP, and i.MX8MM**.

As a **server**, it records the display and streams it. As a **client**, it plays RTSP/RTP streams and can send keyboard/mouse input back to the server for remote control.

---
## Key Features

### 1. Multiple Stream Types
- Supports **RTSP (default)**, **RTP**, **HLS**, and **file recording**.
- Stream payload options:
  - RTP **MPEG‑2**
  - RTP **H.264** (not supported by some desktop video players for RTP stream)

### 2. Remote Control Support
- Enabled by default on **i.MX boards**.
- Windows only supports **screen sharing** (no remote control).
- Disable using: `--remote_control 0`

### 3. Audio Support
- Optional audio streaming and recording.
- Enable with: `--audio_record 1`
- Select audio device using: `--audio_card`. Default: WM-series cards

### 4. Custom Pipelines
- Both client and server modes allow custom pipelines for advanced use cases.

### 5. PipeWire Optimizations
- Supports `keepalive_time` property on `pipewiresrc` to reduce bandwidth when the display is idle.

---
## Common Usage Scenarios
**SERVER_IP** = IP address of the server
**CLIENT_IP** = IP address of the client

---
# RTSP Usage

### 1. RTSP Remote Desktop (Video Only, Remote Control Enabled)
**Server:**
```
/usr/bin/remote_desktop.py
```
**Client:**
```
/usr/bin/remote_desktop.py --mode 0 --server_ip SERVER_IP
```

### 2. RTSP with Audio + Remote Control
**Server:**
```
/usr/bin/remote_desktop.py --audio_record 1
```
**Client:**
```
/usr/bin/remote_desktop.py --mode 0 --server_ip SERVER_IP --audio_record 1
```

### 3. RTSP Without Remote Control (Video Only)
**Server:**
```
/usr/bin/remote_desktop.py --remote_control 0
```
**Client:**
- **i.MX board:**
```
/usr/bin/remote_desktop.py --mode 0 --server_ip SERVER_IP --remote_control 0
```
- **PC:**
```
vlc rtsp://SERVER_IP:8554/test
```

### 4. RTSP Without Remote Control (Audio + Video)
**Server:**
```
/usr/bin/remote_desktop.py --audio_record 1 --remote_control 0
```
**Client:**
- **i.MX board:**
```
/usr/bin/remote_desktop.py --mode 0 --server_ip SERVER_IP --audio_record 1 --remote_control 0
```
- **PC:**
```
vlc rtsp://SERVER_IP:8554/test
```

---
# RTP Usage

### 1. RTP Remote Desktop (Video Only, Remote Control Enabled)
**Server:**
```
/usr/bin/remote_desktop.py --record_type 1 --rtp_ip CLIENT_IP
```
**Client:**
```
/usr/bin/remote_desktop.py --mode 0 --record_type 1 --server_ip SERVER_IP
```

### 2. RTP with Audio + Remote Control
**Server:**
```
/usr/bin/remote_desktop.py --record_type 1 --rtp_ip CLIENT_IP --audio_record 1
```
**Client:**
```
/usr/bin/remote_desktop.py --mode 0 --record_type 1 --server_ip SERVER_IP --audio_record 1
```

### 3. RTP Without Remote Control (Video Only)
**Server:**
```
/usr/bin/remote_desktop.py --record_type 1 --rtp_ip CLIENT_IP --remote_control 0
```
**Client:**
- **i.MX board:**
```
/usr/bin/remote_desktop.py --mode 0 --record_type 1 --server_ip SERVER_IP --remote_control 0
```
- **PC:**
```
vlc rtp://@SERVER_IP:1234
```

### 4. RTP Without Remote Control (Audio + Video)
**Server:**
```
/usr/bin/remote_desktop.py --record_type 1 --rtp_ip CLIENT_IP --audio_record 1 --remote_control 0
```
**Client:**
- **i.MX board:**
```
/usr/bin/remote_desktop.py --mode 0 --record_type 1 --server_ip SERVER_IP --audio_record 1 --remote_control 0
```
- **PC:**
```
vlc rtp://@SERVER_IP:1234
```

---
# HLS Usage
### HLS (No Remote Control)
**Server:**
```
/usr/bin/remote_desktop.py --record_type 2 --remote_control 0
```
**Client (PC):**
```
vlc http://SERVER_IP:9999/screen_play.m3u8
```

---
# Record to Local File (with Audio)
```
/usr/bin/remote_desktop.py --record_type 3 --audio_record 1
```

---
# User-defined Stream Pipeline Usage
### 1. User-defined pipeline on server (RTP video only stream, Remote Control Enabled)
**Server:**
```
/usr/bin/remote_desktop.py --record_type 1 --rtp_ip CLIENT_IP --pipeline "user-defined pipeline"
```
**Client (PC):**
```
/usr/bin/remote_desktop.py --mode 0 --record_type 1 --server_ip SERVER_IP
```

### 2. User-defined pipeline on client (RTP video only stream, Remote Control Enabled)
**Server:**
```
/usr/bin/remote_desktop.py --record_type 1 --rtp_ip CLIENT_IP
```
**Client (PC):**
```
/usr/bin/remote_desktop.py --mode 0 --record_type 1 --server_ip SERVER_IP --pipeline "user-defined pipeline"
```

---
# Help
```
./pipewire_recorder.py --help
```

---
# remote_input Tool
`remote_input` provides HID-based remote control for keyboard and mouse.

### Client Example
```
/usr/bin/remote_input -r 0 -o SERVER_IP
```
### Server Example
```
/usr/bin/remote_input -r 1 -o SERVER_IP
```

### Notes
1. Start **server first**, then the client.
2. `remote_desktop.py` already integrates this feature and running `remote_input` manually is usually unnecessary.

---
