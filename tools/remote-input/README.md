remote_input
================

remote_input is a client-server HID (Human Interface Device) tool
which is designed for remote control. The tool can be configured as
client or server mode.

As a client, it can detect all input devices automatically and support
hotplug detection. It captures the mouse or keyboard data and then send
to server by internet.

As a server, it supports creating virtual mouse and keyboard. It can
receive data from client and then send to native virtual mouse or keyboard.  

Example:
client:  /usr/bin/remote_input -r 0 -o $server_ip
server: /usr/bin/remote_input -r 1 -o $server_ip
Attention: the server program runs first, followed by the client program.