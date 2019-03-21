# General overview

  This document describes, in an increasing level of detail, the connection and protocol between the Deskemes desktop client and the phone app.

  The Deskemes desktop client and phone app need to communicate via various media (TCPIP / WiFi, BlueTooth, USB cable, possibly private cloud connection server in the future). Some of these media are point-to-point connections (based on their functionality), while others are shared between multiple devices. There are protocol layers that simplify things by bringing the two together.


## Shared media: discovery

  When using a shared media, such as a TCPIP connection over WiFi, the first point of order is to find the client and app that want to talk together - discovery. There may be multiple clients and multiple apps in the same shared network, and there could also be malicious adversaries on the network.

  Discovery is enabled by local UDP broadcast. The phone app listens on a predefined UDP port, and the desktop client broadcasts UDP packets periodically. The packet contains protocol identifier ("This is Deskemes v5"), the client public identifier ("This is Deskemes a1b2c3d4-e5f6"), the TCP port on which the main comm service is listening and a flag indicating whether discovery is being done. Discovery mode means that the desktop client is actively looking for new devices (the user is using the Add device wizard to connect to a new device); in regular (non-discovery) mode the desktop client only announces its availability for devices that it has already been paired with.

  Once the phone app decides to talk (knows the client's identifier, or the client requested discovery and the device is willing to be discovered), it opens a TCP connection to the desktop client's main comm service, and the next layer (point-to-point) takes over.


## Point-to-point: Security

  With point-to-point connections, the desktop client is the party that listens for incoming connections, while the device app acts as the client for those connections (i. e. initiates them). When a point-to-point connection is established, the communication first starts un-encrypted. The protocol is a simple request-response dialog, where each party can request (public) information from the other party, such as the pretty-name, the public identifier, public avatar, public key etc. The desktop client provides initiative - it sends the first message, identifying the protocol and requesting identification from the app.

  The public key exchange, although done in cleartext, is specially important for security. Each party maintains a list of public identifiers and their public keys for those connections that have been approved by the user earlier. If the received public identifier of the remote party is not known yet, a special request is to be sent ("need user-verified pairing") and both the desktop client and the phone app will display UI to the user where they ask for approval of the public key; they can present the key as a raw hex dump, cryptographic checksum or (preferably) as randomart. The user can verify if the two keys match in both the desktop client and the device app, thus storing (overwriting) the identifier-key pair in both.

  At any point, either party can decide that they have enough of public information and that they want to initiate encryption. The party sends an encryption request to the other party. The recipient can continue the unencrypted request-response protocol (the initiating party may choose to respond with "error" if they wish) until they want to start encryption too, and they send the encryption request as well. Once the encryption response is sent both ways, the connection starts TLS, with the desktop app being the "server" in TLS terms.

  The TLS first exchanges each party's certificates (self-signed self-generated certificates). The certificates need to use the public keys that were communicated in the unencrypted phase. The point of the pubkey exchange during the unencrypted phase is so that both parties can show the pairing UI if either one of the parties doesn't have a key match - a requirement difficult to implement while handshaking TLS. The assumption is that although an adversary could MITM the unencrypted exchange, they couldn't spoof the certificate anyway so they wouldn't be able to fully enter the TLS phase of the protocol.

  At this point, both parties have a secure and user-approved connection to the other party.

  In order to minimize the attack surface for "confirmation flood" (malicious network device could spam the phone app with many pairing requests that would flood the UI), the phone app will include a setting "do not respond to discovery requests"; with it turned on the app will not respond to discovery requests at all, so it will only be reachable from clients that were already paired.

  The certificates used for communication will be unique for each client and device pair: A desktop client can connect to multiple devices and it will use a different certificate for each device, and similarly a device can connect to multiple desktop clients and it will use a different certificate for each client. This ensures that, if needed, any single connection can be "revoked" without requiring any extra work for any other connections.


## Muxing

  The single encrypted connection we now have needs to encapsulate several internal channels. We'll have a "main channel" that will take commands to open and close other channels, and then "worker channels" for specific use cases. Generally, the desktop client will tell the phone app through the main channel, "open a new filesystem channel", the app will allocate a new channel and respond "ok, the new channel is ID 5". Both parties may excercise QoS over the channels, a good idea being prioritizing the main channel, while putting any filesystem channel in the background. Closing a channel is similarly done via the main channel, from either side of the connection. There is no acknowledgement of receiving the data in the muxer, but the per-channel protocols may introduce their own ack.

  Multiplexing the channels will take the usual <ChannelID><length><data> approach, being a stream-based protocol, rather than datagram-based.


# Technical details

## Discovery

  The phone app listens on two UDP ports, 24816 and 4816 (just in case if one of them is unavailable). The desktop client periodically broadcasts a UDP packet in the following format:

| Data        | Type         | Notes                                    |
| ----------- | ------------ | ---------------------------------------- |
| `Deskemes`  | string       | Protocol identification                  |
| Version     | uint16       | Protocol version (MSB first)             |
| ID length   | uint16       | Public ID length (MSB first)             |
| Public ID   | bytes        | Public ID                                |
| TCP port    | uint16       | The TCP port on which the client listens |
| IsDiscovery | 0x00 or 0x01 | Flag whether a discovery is being made   |

  The numbers are sent big endian - MSB first. The public ID may be any sequence of bytes, minimum length is 16 bytes.

## Point-to-point request-response phase

  After the device connects, the protocol starts in a cleartext request-response protocol. Either peer can send a request; requests are paired to their responses using single-byte IDs, so that a peer can, theoretically, send multiple requests and then process all the responses at once. If a request causes an error, an Error response is sent instead of a regular Response. The packets have the following format:

  Request:

| Data      | Type    | Notes                                                |
| --------- | ------- | ---------------------------------------------------- |
| MsgType   | 0x00    | This is a request                                    |
| RequestID | uint8   | The ID of the request                                |
| ReqType   | 4 bytes | The type of the request (what data is it requesting) |
| MsgLen    | uint16  | The length of the additional data (MSB first)        |
| MsgData   | bytes   | Additional request data                              |

  Response:

| Data      | Type   | Notes                                          |
| --------- | ------ | ---------------------------------------------- |
| MsgType   | 0x01   | This is a response                             |
| RequestID | uint8  | The ID of the request this response belongs to |
| MsgLen    | uint16 | The length of the response data (MSB first)    |
| MsgData   | bytes  | The response data                              |

  Error:

| Data      | Type   | Notes                                            |
| --------- | ------ | ------------------------------------------------ |
| MsgType   | 0x02   | This is an error report                          |
| RequestID | uint8  | The ID of the request this response belongs to   |
| ErrorCode | uint16 | The numerical error code (list below; MSB first) |
| MsgLen    | uint16 | The length of the error message (MSB first)      |
| MsgData   | bytes  | The error message                                |

  Special - TLS start:

| Data      | Type   | Notes                         |
| --------- | ------ | ----------------------------- |
| MsgType   | 0x03   | Everything from now on is TLS |



The numbers are sent big-endian - MSB first. The ReqType is a 4-byte identifier of what data is requested (list of request types is down below). A request can have an additional data. A response has a similar format but doesn't repeat the ReqType. MsgData length for each packet is capped at 64 KiB; the assumption is that this is enough for all messages; no particular fragmentation scheme is specified. If a specified ReqType is not supported by the receiver, it sends an Error response with the error code `ERR_UNSUPPORTED_REQTYPE`.


## Point-to-point request types

  The following section details the request types supported by the protocol.

### Exchange friendly name: `xfnm`

  The sender requests the friendly name of the other party, and at the same time provides theirs. The friendly name is used in the UI to help the users decide which device is the one they want to connect to / pair with.

  The request additional data and the response data have the same following format:

| Data       | Type   | Notes                                       |
| ---------- | ------ | ------------------------------------------- |
| NameLength | uint16 | The length of the Name (MSB first)          |
| Name       | string | The friendly name (NameLength bytes; UTF-8) |
| reserved   | ?      | Any trailing data is currently ignored      |


### Exchange avatar: `xava`

  The sender requests the avatar of the other party, and at the same time sends theirs. An avatar is used in the UI to help the users distinguish the devices.

  The request additional data and the response data have the same following format:

| Data         | Type   | Notes                                             |
| ------------ | ------ | ------------------------------------------------- |
| AvatarLength | uint16 | The length of the Avatar (MSB first)              |
| Avatar       | bytes  | The avatar image (AvatarLength bytes; PNG or JPG) |
| reserved     | ?      | Any trailing data is currently ignored            |

  Note that the avatar data must be shorter than 65534 bytes in order to fit the packet without segmentation. The receiver will auto-detect whether the data is PNG or JPG, based on the format header.


### Exchange Public ID: `xpid`

  The sender requests the public ID of the receiver, while providing theirs. The public ID is used to identify the desktop client instance, or the phone app instance, uniquely. It is any sequence of bytes at least 16 bytes long (see the Discovery section above).

  The request additional data and the response data have the same following format:

| Data     | Type   | Notes                                  |
| -------- | ------ | -------------------------------------- |
| IDLength | uint16 | The length of the ID (MSB first)       |
| ID       | bytes  | The public ID (IDLength bytes)         |
| reserved | ?      | Any trailing data is currently ignored |


### Exchange Public Key: `xpbk`

  The sender requests the public key of the receiver, while providing theirs. The public key is then checked against the certificate presented during TLS; it is exchanged in advance so that both endpoints can present a pairing UI to the user if any of them finds that they don't know the other party yet. For details see also the Security overview and the `pair` request type.

  The request additional data and the response data have the same following format:

| Data      | Type   | Notes                                  |
| --------- | ------ | -------------------------------------- |
| KeyLength | uint16 | The length of the Key (MSB first)      |
| Key       | bytes  | The public key (KeyLength bytes)       |
| reserved  | ?      | Any trailing data is currently ignored |


### Need pairing UI: `pair`

  The sender signals by this request that it doesn't have a valid pairing to the receiver, and they should both present the pairing UI to the user. After the response is sent, the pairing UI is shown to the user. Any MsgData sent in the response is ignored. If an Error (`ERR_NOT_YET`) is sent instead, the UI is not to be shown at all (the remote is not interested in pairing just yet).


### Start TLS: `stls`

  The sender requests that TLS be started on this connection. After the success Response from the receiver, each peer sends a special packet "TLS start" (a single 0x03 byte where ReqType normally is) to indicate where the TLS communication starts (as opposed to leftover Request / Response / Error messages). While waiting for a Response to `stls`, Requests may still be sent, but their Responses may get lost because the receiver could already have sent the 0x03 byte and they cannot respond once they do. If the receiver doesn't wish to start TLS just yet, they can instead respond with an Error message (`ERR_NOT_YET`) and normal protocol flow is restored.


## Encrypted muxing

  Once the connection enters encrypted state via TLS, the communication muxes multiple "channels" into a single data stream. Each channel has a "service" attached to it that further specifies the protocol used on that channel - such as filesystem, phonebook, dialer, messages etc. The services are each documented in a separate document, except for the Channel 0, the "main channel", which is documented down below.

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

  Channel 0 is reserved as the "main channel" that takes commands and opens and closes other channels, as requested. Each channel has a specific kind that dictates the data format. The data sent on Channel 0 uses a request-response protocol similar to the unencrypted protocol: Either peer can send a request; request are paired to their responses using single-byte IDs, so that a peer can, theoretically, send multiple requests and then process all the responses at once. If a request causes an error, an Error response is sent instead of a regular Response. The messages have the following format:

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

| Data      | Type   | Notes                                               |
| --------- | ------ | --------------------------------------------------- |
| MsgType   | 0x02   | This is an error report                             |
| RequestID | uint8  | The ID of the request this response belongs to      |
| ErrorCode | uint16 | The numerical error code (list below; MSB first)    |
| MsgLen    | uint16 | The length of MsgData (MSB first)                   |
| MsgData   | string | The error message (UTF-8; potentially user-visible) |


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


## Error codes

  The following table lists the error codes in both the point-to-point request-response protocol and the Channel 0 protocol. Any other code may be used as well; the following are predefined by the protocol and well understood by both the desktop client and phone app.

| ErrorCode | Name                    | Meaning                                                              |
| ---------:| ----------------------- | -------------------------------------------------------------------- |
|         1 | ERR_UNSUPPORTED_REQTYPE | The ReqType is not supported                                         |
|         2 | ERR_NO_SUCH_SERVICE     | The requested service is not known                                   |
|         3 | ERR_NO_CHANNEL_ID       | All the Channel IDs are used, cannot allocate a new one              |
|         4 | ERR_SERVICE_INIT_FAILED | The service failed to initialize. Channel not allocated              |
|         5 | ERR_NO_PERMISSION       | The phone app needs an (Android) permission first for this operation |
|         6 | ERR_NOT_YET             | The sender doesn't want to TLS or pair yet                           |

  Errors that are violations of the cleartext point-to-point protocol (such as too short PublicID) or have security implications (`stls` Request without a prior `xpbk` Request) should be handled by closing the entire connection. Errors in the TLS Muxing phase should be preferably handled by returning an error code, rather than closing the entire connection.
