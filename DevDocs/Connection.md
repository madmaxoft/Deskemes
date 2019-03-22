# General overview

  This document describes, in an increasing level of detail, the connection and protocol between the Deskemes desktop client and the phone app.

  The Deskemes desktop client and phone app need to communicate via various media (TCPIP / WiFi, BlueTooth, USB cable, possibly private cloud connection server in the future). Some of these media are point-to-point connections (based on their functionality), while others are shared between multiple devices. There are protocol layers that simplify things by bringing the two together.


## Shared media: discovery

  When using a shared media, such as a TCPIP connection over WiFi, the first point of order is to find the client and app that want to talk together - discovery. There may be multiple clients and multiple apps in the same shared network, and there could also be malicious adversaries on the network.

  Discovery is enabled by local UDP broadcast. The phone app listens on a predefined UDP port, and the desktop client broadcasts UDP packets periodically. The packet contains protocol identifier ("This is Deskemes v5"), the client public identifier ("This is Deskemes a1b2c3d4-e5f6"), the TCP port on which the main comm service is listening and a flag indicating whether discovery is being done. Discovery mode means that the desktop client is actively looking for new devices (the user is using the Add device wizard to connect to a new device); in regular (non-discovery) mode the desktop client only announces its availability for devices that it has already been paired with.

  Once the phone app decides to talk (knows the client's identifier, or the client requested discovery and the device is willing to be discovered), it opens a TCP connection to the desktop client's main comm service, and the next layer (point-to-point) takes over.


## Point-to-point: Security

  With point-to-point connections, the desktop client is the party that listens for incoming connections, while the device app acts as the client for those connections (i. e. initiates them). When a point-to-point connection is established, the communication first starts un-encrypted. The protocol is a simple information dump, where each party can send (public) information to the other party, such as the pretty-name, the public identifier, public avatar, public key etc. This phase is used both while performing discovery and for starting a regular connection to an already-paired device. When doing discovery, the desktop client will not send a public key (because it is not yet interested in pairing, it just needs a list of devices to show to the user). Once the user chooses the device, a keypair is generated on the desktop client, the public key is sent to the phone app, so the phone app generates a keypair as well and sends the public key back to the desktop client.

  The public key exchange, although done in cleartext, is specially important for security. Each party maintains a list of public identifiers and their public keys for those connections that have been approved by the user earlier. If the received public identifier of the remote party is not known yet, a special message is to be sent ("need user-verified pairing") and both the desktop client and the phone app will display UI to the user where they ask for approval of the public key; they can present the key as a raw hex dump, cryptographic checksum or (preferably) as randomart. The user can verify if the two keys match in both the desktop client and the device app, thus storing (overwriting) the identifier-key pair in both.

  At any point, either party can decide that they have enough of public information and that they want to initiate encryption. The party sends an encryption request to the other party. The recipient can continue the unencrypted message protocol, but the initiator will not send any more information. After the second peer sends the encryption request as well, the connection starts TLS, with the desktop app being the "server" in TLS terms.

  The TLS first exchanges each party's certificates (self-signed self-generated certificates). The certificates need to use the public keys that were communicated in the unencrypted phase. The point of the pubkey exchange during the unencrypted phase is so that both parties can show the pairing UI if either one of the parties doesn't have a key match - a requirement difficult to implement while handshaking TLS. The assumption is that although an adversary could MITM the unencrypted exchange, they couldn't spoof the certificate anyway so they wouldn't be able to fully enter the TLS phase of the protocol.

  At this point, both parties have a secure and user-approved connection to the other party.

  In order to minimize the attack surface for "confirmation flood" (malicious network device could spam the phone app with many pairing requests that would flood the UI), the phone app will drop any other connections that request pairing while the pairing UI is being shown for a connection. The app will also include a setting "do not respond to discovery requests"; with it turned on the app will not open connection to a client whose public ID it doesn't already know, so it will only be reachable from clients that were already paired.

  The certificates used for communication will be unique for each client and device pair: A desktop client can connect to multiple devices and it will use a different certificate for each device, and similarly a device can connect to multiple desktop clients and it will use a different certificate for each client. This ensures that, if needed, any single connection can be "revoked" without requiring any extra work for any other connections.

  If, at any point, a peer detects abuse of the protocol or some possible breach of security, it may choose to close the connection, and on multiple offences, blacklist the IP address so that it wouldn't even connect to it in the future. The exact rules are up to the app developer.


## Muxing

  The single encrypted connection we now have needs to encapsulate several internal channels. We'll have a "main channel" that will take commands to open and close other channels, and then "worker channels" for specific use cases. Generally, the desktop client will tell the phone app through the main channel, "open a new filesystem channel", the app will allocate a new channel and respond "ok, the new channel is ID 5". Both parties may excercise QoS over the channels, a good idea being prioritizing the main channel, while putting any filesystem channel in the background. Closing a channel is similarly done via the main channel, from either side of the connection. There is no acknowledgement of receiving the data in the muxer, but the per-channel protocols may introduce their own ack.

  Multiplexing the channels will take the usual <ChannelID><length><data> approach, being a stream-based protocol, rather than datagram-based.


# Technical details

  The protocol described here is version 1, so whenever a protocol version needs to be sent, it means the number "1". Future versions may specify ways of handling backward compatibility in the protocol.


## Discovery

  The phone app listens on two UDP ports, `24816` and `4816` (just in case if one of them is unavailable). The desktop client periodically broadcasts a UDP packet to both of those ports, in the following format:

| Data        | Type         | Notes                                    |
| ----------- | ------------ | ---------------------------------------- |
| `Deskemes`  | string       | Protocol identification                  |
| Version     | uint16       | Protocol version (MSB first)             |
| ID length   | uint16       | Public ID length (MSB first)             |
| Public ID   | bytes        | Public ID                                |
| TCP port    | uint16       | The TCP port on which the client listens |
| IsDiscovery | 0x00 or 0x01 | Flag whether a discovery is being made   |

  The numbers are sent big endian - MSB first. The public ID may be any sequence of bytes, minimum length is 16 bytes.


## Point-to-point cleartext phase

  After the device connects, the protocol starts in cleartext. Either peer can send a message containing a piece of information or an instruction. The very first information piece needs to be the protocol identification string (see `dsms` below). The packets have the following format:

| Data      | Type    | Notes                                                |
| --------- | ------- | ---------------------------------------------------- |
| MsgType   | 4 bytes | The type of the message                              |
| MsgLen    | uint16  | The length of the additional data (MSB first)        |
| MsgData   | bytes   | Additional data (MsgLen bytes)                       |

  The numbers are sent big-endian - MSB first. The MsgType is a 4-byte identifier of what piece of information or instruction the message contains (a list of types is down below). MsgData length for each packet is capped at 64 KiB; the assumption is that this is enough for all messages; no particular fragmentation scheme is specified. If a specified MsgType is not supported by the receiver, it ignores the message and continues processing the next message.

  The MsgType is interpreted as 4 successive bytes and documented as 4 ASCII bytes in the documentation below. This means that for example the `dsms` message type should send the four bytes 0x64 0x73 0x6d 0x73 (`d`, `s`, `m`, `s`) as the message type.


## Point-to-point message types

  The following section details the message types supported by the protocol.


### Protocol identification: `dsms`

  The very first message that needs to be sent from each peer. This ensures that the peer knows that they connected to the right service and don't try spamming e. g. a http server with Deskemes traffic. The message data is in the following format:

| Data       | Type   | Notes                            |
| ---------- | ------ | -------------------------------- |
| `Deskemes` | bytes  | Literal string                   |
| Version    | uint16 | The protocol version (MSB first) |

  The desktop client sends the identification first; the phone app should check the `Deskemes` literal string, and if present, it should send their `dsms` packet, even if they don't implement the client's protocol version. Future protocol versions may specify handling backward compatibility, for now the only rule is to send the version and expect the other party to follow that version of the protocol. If there ever is a change in the protocol that would break this assumption, the higher-version peer will disconnect the socket.


### Friendly name: `fnam`

  The sender provides its friendly name. The friendly name is used in the UI to help the users decide which device is the one they want to connect to / pair with. It is also to be shown in the pairing UI. The message data contains an UTF-8 representation of the name.


### Avatar: `avtr`

  The sender provides its avatar. An avatar is used in the UI to help the users distinguish the devices. The message data contains the avatar picture, either in PNG or JPG format. The receiver will auto-detect the format, based on its header. Note that the avatar data must be shorter than 65536 bytes in order to fit the packet.


### Public ID: `pubi`

  The sender provides its public ID. The public ID is used to identify the desktop client instance, or the phone app instance, uniquely. It is any sequence of bytes at least 16 bytes long (see the Discovery section above). The message data is the public ID, as raw bytes.


### Public Key: `pubk`

  The sender provides its public key. The public key is later checked against the certificate presented during TLS; it is exchanged in advance so that both endpoints can present a pairing UI to the user if any of them finds that they don't know the other party yet. For details see also the Security overview and the `pair` message type.

  The message data is the raw public key, in DER encoding.

  Note that the `pubk` is sent from the desktop client first; the device only generates its keypair (if not generated yet) after receiving this message.


### Need pairing UI: `pair`

  The sender signals by this message that it doesn't have a valid pairing to the receiver, and they should both present the pairing UI to the user. The sender is already showing the UI to the user.


### Start TLS: `stls`

  The sender requests that TLS be started on this connection. The sender will not send any more cleartext messages, the next data will be the TLS handshake. The receiver may still send more cleartext messages. When the receiver decides to go TLS as well, it sends the `stls` message, too, and the TLS handshake commences.


## Encrypted muxing

  Once the connection enters encrypted state via TLS, the communication muxes multiple "channels" into a single data stream. Each channel has a "service" attached to it that further specifies the protocol used on that channel - such as filesystem, phonebook, dialer, messages, notifications, status etc. The services are each documented in a separate document, except for the Channel 0, the "main channel", which is documented down below.

  Since the connection is encrypted at this point, the protocol no longer focuses on security, but on functionality and easy troubleshooting.

  The protocol uses the following message format:

| Name       | Type   | Meaning                                             |
| ---------- | ------ | --------------------------------------------------- |
| ChannelID  | uint16 | The channel for which the data is meant (MSB first) |
| DataLength | uint16 | The length of the Data (MSB first)                  |
| Data       | bytes  |                                                     |

  The maximum amount of data that can be sent in a single message is 64 KiB.

  If a message is received for a non-existent ChannelID, it is silently dropped.


## Channel 0 messages

  Channel 0 is reserved as the "main channel" that takes commands and opens and closes other channels, as requested. Each channel has a specific kind that dictates the data format. The data sent on Channel 0 uses a request-response protocol: Either peer can send a request; request are paired to their responses using single-byte IDs, so that a peer can, theoretically, send multiple requests and only then process all the responses at once. If a request causes an error, an Error response is sent instead of a regular Response. The messages have the following format:

  Request:

| Data      | Type    | Notes                                                |
| --------- | ------- | ---------------------------------------------------- |
| MsgType   | 0x00    | This is a request                                    |
| RequestID | uint8   | The ID of the request                                |
| ReqType   | 4 bytes | The type of the request (what data is it requesting) |
| MsgLen    | uint16  | The length of MsgData (MSB first)                    |
| MsgData   | bytes   | Additional request data                              |

  Response:

| Data      | Type   | Notes                                          |
| --------- | ------ | ---------------------------------------------- |
| MsgType   | 0x01   | This is a response                             |
| RequestID | uint8  | The ID of the request this response belongs to |
| MsgLen    | uint16 | The length of MsgData (MSB first)              |
| MsgData   | bytes  | The response data                              |

  Error:

| Data      | Type   | Notes                                                     |
| --------- | ------ | --------------------------------------------------------- |
| MsgType   | 0x02   | This is an error report                                   |
| RequestID | uint8  | The ID of the request this response belongs to            |
| ErrorCode | uint16 | The numerical error code (MSB first, list of codes below) |
| MsgLen    | uint16 | The length of MsgData (MSB first)                         |
| MsgData   | string | The error message (UTF-8; potentially user-visible)       |


The numbers are sent big-endian - MSB first. The ReqType is a 4-byte identifier of what data is requested (list of request types is down below). A request can have an additional data. A response has a similar format but doesn't repeat the ReqType. Data length for each packet is capped at 64 KiB - 12 (so that the entire message fits into a single mux message); the assumption is that this is enough for all messages; no particular fragmentation scheme is specified. If a specified ReqType is not supported by the receiver, it sends an Error response with the error code `ERR_UNSUPPORTED_REQTYPE`.


## Channel 0 request types

  The following requests can be sent over Channel 0:


### `open`: Open a new channel

  Opens a new channel to the specified service. A new channel is allocated a free ID and the service is assigned to it. When initializing the service, an additional data may be specified, the service consumes that data and uses it for initialization.

  The request additional data has the following format:

| Data       | Type   | Notes                                                      |
| ---------- | ------ | ---------------------------------------------------------- |
| SvcLength  | uint16 | The length of the Service (MSB first)                      |
| Service    | string | The name of the service to attach (SvcLength bytes)        |
| DataLength | uint16 | The length of the Data (MSB first)                         |
| Data       | bytes  | The initialization data for the service (DataLength bytes) |
| reserved   | ?      | Any trailing data is currently ignored                     |

  If the opening succeeds, the Response data has the following format:

| Data      | Type   | Notes                                  |
| --------- | ------ | -------------------------------------- |
| ChannelID | uint16 | The ID of the new Channel (MSB first)  |
| reserved  | ?      | Any trailing data is currently ignored |

  If the service is not known, the `ERR_NO_SUCH_SERVICE` Error is returned. If there are no free channel IDs, the `ERR_NO_CHANNEL_ID` Error is returned. If the service failed to initialize, the `ERR_SERVICE_INIT_FAILED` Error is returned.


### `clse`: Close an existing channel

  Closes an existing channel. If the specified channel doesn't exist, returns `ERR_NO_SUCH_CHANNEL`.

  The request additional data has the following format:

| Data      | Type   | Notes                                  |
| --------- | ------ | -------------------------------------- |
| ChannelID | uint16 | The Channel to close (MSB first)       |
| reserved  | ?      | Any trailing data is currently ignored |


### `ping`: Request a response

  Simply requests a `pong` response, used for keeping the connection alive (sent every 30 seconds) and possibly to measure the connection latency. In Deskemes this is sent only by the desktop client, but both parties accept it.

  Any additional data sent via `ping` is to be repeated in the `pong` response.


### `pong`: Responding to a ping

  The `pong` response is sent after a `ping` request is received. The additional data from the `ping` is repeated in the `pong`.


## Error codes

  The following table lists the error codes in the Channel 0 protocol. Any other code may be used as well; the following are predefined by the protocol and well understood by both the desktop client and phone app.

| ErrorCode | Name                    | Meaning                                                              |
| ---------:| ----------------------- | -------------------------------------------------------------------- |
|         1 | ERR_UNSUPPORTED_REQTYPE | The ReqType is not supported                                         |
|         2 | ERR_NO_SUCH_SERVICE     | The requested service is not known                                   |
|         3 | ERR_NO_CHANNEL_ID       | All the Channel IDs are used, cannot allocate a new one              |
|         4 | ERR_SERVICE_INIT_FAILED | The service failed to initialize. Channel not allocated              |
|         5 | ERR_NO_PERMISSION       | The phone app needs an (Android) permission first for this operation |
|         6 | ERR_NOT_YET             | The sender doesn't want to TLS or pair yet                           |


## Design decisions:

  The following section documents the decisions that were taken when designing the protocol.


### Why not use TLS on the connection from the very beginning?

  The TLS requires the use of certificates; since each pair of [device, client] uses a different certificate, the peers wouldn't know which certificate to present. They need to first receive the PublicID of the other party in order to select the proper certificate.


### Why should the phone app delay its keypair generation?

  The phone app should delay generating its keypair until it receives the pubkey from the desktop client. This is mainly due to the same protocol being used for discovery - we don't want to generate a keypair during discovery, but only when the user really wants to connect (using the New device wizard in the desktop client). The keypair generation is possibly a lengthy and CPU-intensive operation, so we want to do it only after we really know that we need it.


### This muxing (on the TLSed connection) seems familiar?

  Yes, the inspiration comes from ADB (Android Debug Bridge) muxing multiple connections over a single link. ADB doesn't have `ping` because it was originally transported by USB and therefore didn't need keepalives.
