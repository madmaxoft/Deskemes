# Main architecture

Deskemes consists of a desktop client (runs on a computer) and a phone app (runs on Android, possibly on iOS in the future). Those two connect together over TCPIP (WiFi), BlueTooth or USB cable. The desktop client provides the UI on the computer, the phone app provides the executive on the phone. For connection architecture details (protocol, multiplexing, encryption, discovery), see Connection.md.

# Security

The connection between the desktop client and the phone app needs to be secure, because it may be routed through possibly-malicious network environments. TLS with public-key cryptography is used as the basis for the security; the phone and the desktop need an initial "pairing" with user confirmation in order to start communicating. After all, we're handling the most personal data one could have.
