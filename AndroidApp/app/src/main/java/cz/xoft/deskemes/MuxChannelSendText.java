package cz.xoft.deskemes;

import android.telephony.SmsManager;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.ArrayList;





class MuxChannelSendText
	extends Connection.MuxChannel
{
	private static final String TAG = "MuxChannelSendText";

	// Message types:
	private static final int msgSend   = 0x73656e64;  // `send`
	private static final int msgDivide = 0x64766465;  // `dvde`

	// Response types:
	private static final byte respError   = 0x01;
	private static final byte respSuccess = 0x02;

	// Error codes:
	private static final short ERR_UNSUPPORTED_REQTYPE = 0x01;
	private static final short ERR_MALFORMED_REQUEST   = 0x07;
	private static final short ERR_SENDING_FAILED      = 0x1000;
	private static final short ERR_DIVIDING_FAILED     = 0x1001;





	MuxChannelSendText(Connection aConnection)
	{
		super(aConnection);
	}





	@Override
	void processMessage(byte[] aMessage)
	{
		if (aMessage.length < 4)
		{
			Log.d(TAG, "Received a too-short message: " + aMessage.length);
			return;
		}
		int msgType = Utils.decodeBE32(aMessage, 0);
		switch (msgType)
		{
			case msgSend:
			{
				processSendMessage(aMessage);
				break;
			}
			case msgDivide:
			{
				processDivideMessage(aMessage);
				break;
			}
			default:
			{
				sendError(ERR_UNSUPPORTED_REQTYPE, "Unknown message type");
			}
		}
	}





	/** Processes a `send` message, sending a text message. */
	private void processSendMessage(byte[] aMessage)
	{
		ByteArrayReader bar = new ByteArrayReader(aMessage, 4);
		String recipient, message;
		try
		{
			recipient = bar.readBE16LString();
			message = bar.readBE16LString();
		}
		catch (ByteArrayReader.DataEndReachedException exc)
		{
			Log.d(TAG, "Failed to read entire message", exc);
			sendError(ERR_MALFORMED_REQUEST, "The protocol message is incomplete");
			return;
		}

		try
		{
			SmsManager smsManager = SmsManager.getDefault();
			smsManager.sendTextMessage(recipient, null, message, null, null);
		}
		catch (Exception exc)
		{
			Log.d(TAG, "Failed to send SMS.", exc);
			sendError(ERR_SENDING_FAILED, "Sending failed: " + exc.getMessage());
			return;
		}

		sendSuccess();
	}





	/** Processes a `dvde` message, dividing a text message into parts. */
	private void processDivideMessage(byte[] aMessage)
	{
		String message = Utils.utf8ToString(aMessage, 4, aMessage.length - 4);
		ArrayList<String> divided;
		try
		{
			SmsManager smsManager = SmsManager.getDefault();
			divided = smsManager.divideMessage(message);
		}
		catch (Exception exc)
		{
			Log.d(TAG, "Failed to send SMS.", exc);
			sendError(ERR_DIVIDING_FAILED, "Dividing failed: " + exc.getMessage());
			return;
		}

		// Serialize the divided messages:
		ByteArrayOutputStream resp = new ByteArrayOutputStream();
		resp.write(respSuccess);
		for (String d: divided)
		{
			try
			{
				resp.write(Utils.encodeBE16((short)d.length()));
				resp.write(Utils.stringToUtf8(d));
			}
			catch (Exception exc)
			{
				sendError(ERR_DIVIDING_FAILED, "Failed to construct a response: " + exc.getMessage());
				return;
			}
		}
		sendMessage(resp.toByteArray());
	}





	/** Sends a success message with no data. */
	private void sendSuccess()
	{
		sendMessage(new byte[]{respSuccess});
	}





	/** Sends an error response with the specified error code and message. */
	private void sendError(short aErrCode, String aErrMsg)
	{
		ByteArrayOutputStream resp = new ByteArrayOutputStream();
		resp.write(respError);
		try
		{
			resp.write(Utils.encodeBE16(aErrCode));
			resp.write(Utils.stringToUtf8(aErrMsg));
		}
		catch (IOException exc)
		{
			Log.d(TAG, "Failed to serialize error response.", exc);
			// Send the error message anyway
		}
		sendMessage(resp.toByteArray());
	}





	@Override
	void initialize(byte[] aServiceInitData)
	{
		// No initialization needed
	}
}
