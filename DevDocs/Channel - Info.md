== The Info channel ==

  The Info channel is used to query overall status information from the device, such as battery level, signal strength, carrier, phone number, IMEI etc. Some of this information makes sense to query just once, other parts are likely to be queried periodically. The protocol is simple enough to accommodate both use-cases.

  The desktop client issues requests for values, the phone app replies with values. The protocol doesn't provide a dialog matching, a reply doesn't have any linkage to the request. When the client requests a value, it simply means "give me the value as soon as you can", and the app replying means "this is the value at current time". The app needn't respond to a particular value (eg. if it doesn't know or have such a value) and there's no way for the client to know whether the value will come or not.

  The values are assigned a 4-byte identifier (represented here as 4 ASCII characters), for example, "batl" for battery level. A value can hold an unsigned integer (8-, 16-, 32- and 64-bit), a fixed-point number (16.16-bit or 32.32-bit) or a string. The datatype is transmitted along with the value and the protocol doesn't restrict the datatype from changing from one query to another, although practically no such use-case makes sense.

  Any initialization data sent to the channel at its creation is ignored.


=== Request format ===

  The desktop client simply sends a message that is a multiple of 4 bytes long. Each successive 4-byte value is interpreted as an identifier mask, where all characters are interpreted as-is, only the question-mark character is a wildcard that matches any character. The app then matches the mask against all the value's identifiers it supports and sends all matched values to the client.

| Requested | Replied                     |
| --------- | --------------------------- |
| `batl`    | `batl`                      |
| `bat?`    | `batl`, `batm`, `bath`, ... |
| `????`    | All values                  |


=== Response format ===

  The app sends values in the following format:

| Field    | Type / length | Description                           |
| -------- | ------------- | ------------------------------------- |
| Key      | 4 bytes       | The value's identifier                |
| Datatype | 1 byte        | The datatype of the value (see below) |
| Data     | ? bytes       | The value itself                      |

  The supported datatypes are:

| Datatype | Length  | Description            |
| -------- | ------- | ---------------------- |
| 0x01     | 1       | Int8                   |
| 0x02     | 2       | Int16                  |
| 0x03     | 4       | Int32                  |
| 0x04     | 8       | Int64                  |
| 0x05     | 4       | Fixed 16.16            |
| 0x06     | 8       | Fixed 32.32            |
| 0x07     | 2 + len | UInt16 length + string |

  All numbers are sent MSB-first. Fixed-point numbers are represented as signed integers which have been multiplied by their half-width:

| Number | Fixed 16.16  | Fixed 32.32         |
| 3.5    | 0x0003 8000  | 0x00000003 80000000 |
| 7.25   | 0x0007 4000  | 0x00000007 40000000 |
| -3.5   | 0xfffc 8000  | 0xfffffffc 80000000 |
| -7.25  | 0xfff8 c000  | 0xfffffff8 c0000000 |

  This message may be concatenated several times, for different values (if multiple values match the request). If all values that match the request would not fit a single response message (64 KiB), a subset of the values is sent in one message and the rest in following messages - but always one message contains only complete values.


=== Predefined value IDs ===

  The following IDs have a predefined meaning:

| Key             | Meaning                                             |
| --------------- | --------------------------------------------------- |
| `batl`          | Total battery current level, in percent             |
| `batm`          | Total battery minutes remaining                     |
| `btl0` - `btl9` | Battery current level, in percent, in battery 0 - 9 |
| `sist`          | Signal strength, in percent, on the primary SIM     |
| `sis0` - `sis9` | Signal strength, in percent, on SIM0 - SIM9         |
| `wss0` - `wss9` | WiFi signal strength, in percent, on wlan0 - wlan9  |
| `imei`          | The IMEI on the primary SIM                         |
| `ime0` - `ime9` | The IMEI on SIM0 - SIM9                             |
| `imsi`          | The IMSI on the primary SIM                         |
| `ims0` - `ims9` | The IMSI on SIM0 - SIM9                             |
| `carr`          | The carrier of the primary SIM                      |
| `car0` - `car9` | The carrier of SIM0 - SIM9                          |
| `geom`          | The geometry of all the displays (pos, size, dpi)   |
| `uptm`          | The device's uptime, in minutes                     |
| `time`          | The current datetime on the device (unixtime)       |

