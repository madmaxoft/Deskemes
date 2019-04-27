package cz.xoft.deskemes;


import android.util.Log;

import java.io.UnsupportedEncodingException;





class MuxChannelZero
	extends Connection.MuxChannel
{
	private final static byte msg_Request  = 0x00;
	private final static byte msg_Response = 0x01;
	private final static byte msg_Error    = 0x02;

	private final static int req_open = 0x6f70656e;
	private final static int req_clse = 0x636c7365;
	private final static int req_ping = 0x70696e67;

	private final static short ERR_UNSUPPORTED_REQTYPE = 1;
	private final static short ERR_NO_SUCH_SERVICE = 2;
	private final static short ERR_NO_CHANNEL_ID = 3;
	private final static short ERR_SERVICE_INIT_FAILED = 4;
	private final static short ERR_NO_PERMISSION = 5;
	private final static short ERR_NO_SUCH_CHANNEL = 6;

	/** The tag used for logging. */
	private final static String TAG = "MuxChannelZero";

	/** Creates a new instance bound to the specified connection. */
	MuxChannelZero(Connection aConnection)
	{
		super(aConnection);
	}





	// MuxChannel override:
	@Override
	public void processMessage(byte[] aMessage)
	{
		if (aMessage.length < 2)
		{
			Log.d(TAG, "Received a too-short Ch0 message");
			return;
		}
		switch (aMessage[0])
		{
			case msg_Request:  handleRequest(aMessage);  break;
			case msg_Response: handleResponse(aMessage); break;
			case msg_Error:    handleError(aMessage);    break;
			default:
			{
				Log.d(TAG, "Received a message of an unknown type: " + aMessage[0]);
				break;
			}
		}
	}





	/** Handles a Request message. */
	private void handleRequest(byte[] aMessage)
	{
		ByteArrayReader bar = new ByteArrayReader(aMessage);
		int requestType;
		byte requestID;
		try
		{
			requestID = bar.readByte();
			requestType = bar.readBE32();
		}
		catch (ByteArrayReader.DataEndReachedException exc)
		{
			Log.d(TAG, "Malformed data message", exc);
			return;
		}
		switch (requestType)
		{
			case req_open: handleOpenRequest(requestID, aMessage);  break;
			case req_clse: handleCloseRequest(requestID, aMessage, bar); break;
			case req_ping: handlePingRequest(requestID, aMessage);  break;
			default:
			{
				sendError(requestID, ERR_UNSUPPORTED_REQTYPE, "Unsupported request type: `" + Utils.stringFromBE32(requestType) + "`");
				break;
			}
		}
	}





	/** Handles a Response message. */
	private void handleResponse(byte[] aMessage)
	{
		// TODO
		Log.d(TAG, "Received a response: " + Utils.bytesToHex(aMessage));
	}





	/** Handles an Error message. */
	private void handleError(byte[] aMessage)
	{
		if (aMessage.length < 4)
		{
			Log.d(TAG, "Received a malformed error message: " + Utils.bytesToHex(aMessage));
		}
		else
		{
			Log.d(TAG,
				"Received an error: " + ((aMessage[2] << 8) | aMessage[3]) +
				"msg: " + Utils.utf8ToString(aMessage, 4, aMessage.length - 4)
			);
		}
	}





	/** Handles an `open` request: opens a new channel */
	private void handleOpenRequest(byte aRequestID, byte[] aMessage)
	{
		// TODO
		Log.d(TAG, "TODO: Open a channel: " + Utils.utf8ToString(aMessage));
		sendError(aRequestID, ERR_NO_SUCH_SERVICE, "No such service");
	}





	/** Handles a `clse` request: closes an existing channel. */
	private void handleCloseRequest(byte aRequestID, byte[] aMessage, ByteArrayReader aBar)
	{
		short id = 0;
		try
		{
			id = (short)aBar.readBE16();
		}
		catch (ByteArrayReader.DataEndReachedException exc)
		{
			Log.d(TAG, "Failed to read ChannelID from the `clse` request", exc);
		}
		if (id == 0)
		{
			Log.d(TAG, "Tried to close Channel 0");
			sendError(aRequestID, ERR_NO_SUCH_CHANNEL, "Channel 0 cannot be closed");
			return;
		}
		Connection.MuxChannel ch = mConnection.muxChannelByID(id);
		if (ch == null)
		{
			Log.d(TAG, "Tried to close non-existing channel " + id);
			sendError(aMessage[0], ERR_NO_SUCH_CHANNEL, "No such channel.");
			return;
		}
		Log.d(TAG, "Closing channel " + id);
		// TODO: Close the channel
	}




	/** Sends an Error response to the remote. */
	private void sendError(byte aRequestID, short aErrorCode, String aErrorMessage)
	{
		byte[] errMsg;
		try
		{
			errMsg = aErrorMessage.getBytes("UTF-8");
		}
		catch (UnsupportedEncodingException exc)
		{
			Log.d(TAG, "Failed to convert error message to UTF-8", exc);
			errMsg = new byte[1];
			errMsg[0] = 0;
		}
		int len = errMsg.length;
		byte[] resp = new byte[len + 4];
		resp[0] = msg_Error;
		resp[1] = aRequestID;
		resp[2] = (byte) (aErrorCode / 256);
		resp[3] = (byte) (aErrorCode % 0xff);
		System.arraycopy(resp, 4, errMsg, 0, len);
		sendMessage(resp);
	}





	/** Handles a `ping` request: sends back a response with the same additional data. */
	private void handlePingRequest(byte aRequestID, byte[] aMessage)
	{
		byte[] resp = new byte[aMessage.length - 4];
		resp[0] = msg_Response;
		resp[1] = aRequestID;
		int len = resp.length - 2;
		System.arraycopy(aMessage, 6, resp, 2, len);
		sendMessage(resp);
	}


}
