package cz.xoft.deskemes;


import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.UnsupportedEncodingException;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;





/** A simple wrapper over byte[] that supports the notion of current position and can read various
datatypes from the data, beginning at that position.
If any of the read operations attempt to read past the end of the data, the DataEndReachedException
is thrown. Once this happens, the reader is in an undefined state and no further operations should
be attempted on it. */
public class ByteArrayReader
{
	/** The tag used for logging. */
	private final String TAG = "ByteArrayReader";

	/** The data that is being read through this reader. */
	private byte[] mData;

	/** The length of mData. */
	private int mLength;

	/** The index of the next byte that will be consumed by the next read operation. */
	private int mIndex;





	/** Exception that is thrown when attempting to read past the data's end. */
	class DataEndReachedException extends Exception
	{
	};





	/** Creates a new instance of the reader that processes the specified data. */
	ByteArrayReader(byte[] aData)
	{
		mData = aData;
		mLength = aData.length;
		mIndex = 0;
	}





	/** Creates a new instance of the reader that processes the specified data, starting at the specified index. */
	ByteArrayReader(byte[] aData, int aInitialIndex)
	{
		mData = aData;
		mLength = aData.length;
		mIndex = aInitialIndex;
	}





	byte readByte() throws DataEndReachedException
	{
		if (mIndex >= mLength)
		{
			throw new DataEndReachedException();
		}
		mIndex += 1;
		return mData[mIndex - 1];
	}





	/** Reads a two-byte big-endian unsigned number from the data. */
	short readBE16() throws DataEndReachedException
	{
		if (mIndex + 1 > mLength)
		{
			throw new DataEndReachedException();
		}
		short val = Utils.decodeBE16(mData, mIndex);
		mIndex += 2;
		return val;
	}





	/** Reads a four-byte big-endian unsigned number from the data. */
	int readBE32() throws DataEndReachedException
	{
		if (mIndex + 3 > mLength)
		{
			throw new DataEndReachedException();
		}
		int val = Utils.decodeBE32(mData, mIndex);
		mIndex += 4;
		return val;
	}





	/** Reads a blob prefixed by 2 BE bytes specifying its length. */
	byte[] readBE16LBlob() throws DataEndReachedException
	{
		int len = readBE16() & 0xffff;
		if (mIndex + len > mLength)
		{
			throw new DataEndReachedException();
		}
		int start = mIndex;
		mIndex += len;
		return Arrays.copyOfRange(mData, start, start + len);
	}




	/** Reads a UTF-8 string prefixed by 2 BE bytes specifying its length. */
	String readBE16LString() throws DataEndReachedException
	{
		byte[] blob = readBE16LBlob();
		try
		{
			return new String(blob, "UTF-8");
		}
		catch (UnsupportedEncodingException exc)
		{
			Log.d(TAG, "Failed to convert UTF-8 to string", exc);
			return "";
		}
	}





	/** Reads a single byte and interprets it as a boolean value. */
	boolean readBoolean() throws DataEndReachedException
	{
		if (mIndex >= mLength)
		{
			throw new DataEndReachedException();
		}
		mIndex += 1;
		return (mData[mIndex - 1] != 0);
	}





	/** Compares the next bytes to read with the specified string, returns true if matching,
	false if not / not enough bytes in the input.
	Moves the current index past the end of the string.
	If false is returned, the current index may be anywhere between the original position and the string length. */
	boolean checkAsciiString(String aString)
	{
		byte[] toCompare;
		try
		{
			toCompare = aString.getBytes("UTF-8");
		}
		catch (Exception exc)
		{
			Log.e(TAG, "Failed to convert string to UTF-8", exc);
			return false;
		}
		if (toCompare.length + mIndex >= mLength)
		{
			return false;
		}
		int len = toCompare.length;
		for (int i = 0; i < len; ++i)
		{
			if (toCompare[i] != mData[i + mIndex])
			{
				return false;
			}
		}
		mIndex += len;
		return true;
	}
}
