# Connection

  This document describes, in an increasing level of detail, the connection and protocol between the Deskemes desktop client and the phone app.

  The Deskemes desktop client and phone app need to communicate via various media (TCPIP / WiFi, BlueTooth, USB cable, possibly private cloud connection server in the future). Some of these media are point-to-point connections (based on their functionality), while others are shared between multiple devices. There are protocol layers that simplify things by bringing the two together.


## Shared media: discovery

  When using a shared media, such as a TCPIP connection over WiFi, the first point of order is to find the client and app that want to talk together - discovery. There may be multiple clients and multiple apps in the same shared network, and there could also be malicious adversaries on the network.

  Discovery is enabled by local UDP broadcast. Both the client and the app listen on a predefined UDP port, and both broadcast packets to the counter-part port when joining the network / starting the client / periodically. The packet contains protocol identifier ("This is Deskemes v5") and the client or phone public identifier ("This is phone a1b2c3d4-e5f6"). The desktop client also sends the TCP port on which the main comm service is listening and a flag indicating whether discovery is being done. Discovery mode means that the desktop client is actively looking for new devices (the user is using the Add device wizard to connect to a new device); in regular (non-discovery) mode the desktop client only announces its availability for devices that it has already been paired with.

  Once the device decides to talk (knows the client's identifier, or the client requested discovery), it opens a TCP connection to the desktop client's main comm service, and the next layer (point-to-point) takes over.


## Point-to-point: Security

  With point-to-point connections, the desktop client is the party that listens for incoming connections, while the device app acts as the client for those connections (i. e. initiates them). When a point-to-point connection is established, the communication first starts un-encrypted. The protocol is a simple request-response dialog, where each party can request (public) information from the other party, such as the pretty-name, the public identifier, public avatar, public key etc. The desktop client provides initiative - it sends the first message, identifying the protocol and requesting identification from the app.

  The public key exchange, although done in cleartext, is specially important. Each party maintains a list of public identifiers and their public keys for those connections that have been approved by the user earlier. If the received public identifier of the remote party is not known yet, a special request is to be sent and both the desktop client and the phone app will display UI to the user where they ask for approval of the public key; they can present the key as a raw hex dump, cryptographic checksum or (preferably) as randomart. The user can verify if the two keys match in both the desktop client and the device app, thus storing (overwriting) the identifier-key pair in both.

  At any point, either party can decide that they have enough of public information and that they want to initiate encryption. The party sends an encryption request to the other party. The recipient can continue the unencrypted request-response protocol (the initiating party can respond with "error" if they wish) until they want to start encryption too, and they send the encryption request as well. Once the encryption response is sent both ways, the connection starts TLS, with the desktop app being the "server" in TLS terms.

  The TLS first exchanges each party's certificates (self-signed self-generated certificates). The certificate needs to use the public key that was communicated in the unencrypted phase. The point of the pubkey exchange during the unencrypted phase is so that if both parties can show the pairing UI if either one of the parties doesn't have a key match - a requirement difficult to implement while handshaking TLS. The assumption is that although an adversary could MITM the unencrypted exchange, they couldn't spoof the certificate anyway so they wouldn't be able to fully enter the TLS phase of the protocol.

  At this point, both parties have a secure and user-approved connection to the other party.


## Muxing

  The single encrypted connection we now have needs to encapsulate several internal channels. We'll have a "main channel" that will take commands to open and close other channels, and then "worker channels" for specific use cases. Generally, the desktop client will tell the phone app through the main channel, "open a new filesystem channel", the app will allocate a new channel and respond "ok, the new channel is ID 5". Both parties may excercise QoS over the channels, a good idea being prioritizing the main channel, while putting any filesystem channel in the background. Closing a channel is similarly done via the main channel, from either side of the connection. There is no acknowledgement of receiving the data in the muxer, but the per-channel protocols may introduce their own ack.

  Multiplexing the channels will take the usual <ChannelID><length><data> approach, being a stream-based protocol, rather than datagram-based.

