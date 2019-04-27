package cz.xoft.deskemes;


import android.util.Log;

import java.io.UnsupportedEncodingException;
import java.nio.BufferOverflowException;
import java.nio.ByteBuffer;





class PacketBuilder
{
	/** The tag used for logging. */
	private final static String TAG = "PacketBuilder";

	/** The buffer that stores the actual data. */
	private ByteBuffer mUnderlyingBuffer;





	public PacketBuilder(ByteBuffer aUnderlyingBuffer)
	{
		mUnderlyingBuffer = aUnderlyingBuffer;
	}




	/** Writes the ASCII literal to the packet.
	Use this only for ASCII strings (characters 0x00 - 0x7f). */
	public void writeAsciiLiteral(String aString) throws BufferOverflowException
	{
		int len = aString.length();
		for (int i = 0; i < len; ++i)
		{
			mUnderlyingBuffer.put((byte)(aString.charAt(i) & 0xff));
		}
	}




	/** Writes the Utf8-encoded string prefixed with a 2-byte length. */
	public void writeBE16LString(String aString) throws BufferOverflowException
	{
		byte[] bytes;
		try
		{
			bytes = aString.getBytes("UTF-8");
		}
		catch (UnsupportedEncodingException exc)
		{
			Log.d(TAG, "Failed to convert string to UTF-8", exc);
			return;
		}
		int len = bytes.length;
		if (len > 65535)
		{
			throw new BufferOverflowException();
		}
		mUnderlyingBuffer.putShort((short)len);
		mUnderlyingBuffer.put(bytes);
	}
}
