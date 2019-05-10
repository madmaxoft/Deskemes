# The SMS send channel

  Channel type identifier: `sms.send`

  The SMS send channel is used to send text messages. The desktop client sends requests to send a text message, and the phone app sends the messages, or responds with an error.

  The service also provides utility function for dividing long messages. The desktop client may use this functionality to indicate to the user how many SMSs their message will take, while the user is typing.

  The protocol is synchronous-dialog-based, each request is matched to a response and the responses come out in the same order as the requests had come in.

  Any initialization data sent to the channel at its creation is ignored.


## Request format

  The desktop client sends one of the following requests:

  Send SMS request:

| Field    | Type / length | Description                                                     |
| -------- | ------------- | --------------------------------------------------------------- |
| ReqType  | 4 bytes       | `send`                                                          |
| DstLen   | 2 bytes       | The length of the Dst field (MSB first)                         |
| Dst      | string        | The destination for the message (recipient phone number; UTF-8) |
| TxtLen   | 2 bytes       | The length of the Txt field (MSB first)                         |
| Txt      | string        | The actual message to send (UTF-8)                              |

  Divide message request:

| Field    | Type / length | Description                             |
| -------- | ------------- | --------------------------------------- |
| ReqType  | 4 bytes       | `dvde`                                  |
| TxtLen   | 2 bytes       | The length of the Txt field (MSB first) |
| Txt      | string        | The actual message to divide (UTF-8)    |


## Response format

  For each request the app sends a response, either a Success or a Failure.

  Success:

| Field    | Type / length | Description            |
| -------- | ------------- | ---------------------- |
| RespType | 1 bytes       | 0x01 = success         |
| Data     | ? bytes       | Possible returned data |

  For the `send` request the response contains no data. For the `dvde` request the response data contains the divided messages, each message prefixed by 2-byte length:

| Field    | Type / length | Description                             |
| -------- | ------------- | --------------------------------------- |
| TxtLen   | 2 bytes       | The length of the Txt field (MSB first) |
| Txt      | string        | The divided message part (UTF-8)        |


  Failure:

| Field    | Type / length | Description                    |
| -------- | ------------- | ------------------------------ |
| RespType | 1 bytes       | 0x02 = failure                 |
| ErrCode  | 2 bytes       | Error code (MSB first)         |
| ErrMsg   | ? bytes       | Possible error message (UTF-8) |
