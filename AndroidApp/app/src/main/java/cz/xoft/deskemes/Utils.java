package cz.xoft.deskemes;


import android.util.Log;

import java.io.UnsupportedEncodingException;





public class Utils
{
	/** The tag used for logging. */
	private static String TAG = "Utils";





	/** Clamps the given value to the given bounds. */
	public static int clamp(int aValue, int aMin, int aMax)
	{
		if (aValue < aMin)
		{
			return aMin;
		}
		if (aValue > aMax)
		{
			return aMax;
		}
		return aValue;
	}





	/** Returns true if the specified range in the byte array contains the bytes comprising the specified string.
	In Lua terms, returns (string.sub(aArray, aStartIdx, aEndIdxIncl) == aString). */
	public static boolean compareByteArrayToString(byte[] aArray, int aStartIdx, int aEndIdxIncl, String aString)
	{
		for (int i = aStartIdx; i < aEndIdxIncl; ++i)
		{
			if (aArray[i] != aString.charAt(i - aStartIdx))
			{
				return false;
			}
		}
		return true;
	}





	private final static char[] hexArray = "0123456789abcdef".toCharArray();





	/** Converts the input bytes into a string that represents each byte with two hex digits. */
	static String bytesToHex(byte[] aBytes)
	{
		int len = aBytes.length;
		char[] hexChars = new char[len * 2];
		for (int j = 0; j < len; ++j)
		{
			int v = aBytes[j] & 0xff;
			hexChars[j * 2] = hexArray[v >>> 4];
			hexChars[j * 2 + 1] = hexArray[v & 0x0f];
		}
		return new String(hexChars);
	}





	/** Converts a long hex-string to a byte array. */
	public static byte[] hexStringToByteArray(String s)
	{
		int len = s.length();
		byte[] data = new byte[len / 2];
		for (int i = 0; i < len; i += 2)
		{
			data[i / 2] = (byte)((Character.digit(s.charAt(i), 16) << 4) | Character.digit(s.charAt(i + 1), 16));
		}
		return data;
	}





	/** Converts the 4-byte number to an ASCII string representing the 4 bytes.
	Eg. 0x64736d73 -> "dsms". */
	static String stringFromBE32(int aValue)
	{
		char[] chars =
		{
			(char)((aValue >> 24) & 0xff),
			(char)((aValue >> 16) & 0xff),
			(char)((aValue >>  8) & 0xff),
			(char)(aValue & 0xff),
		};
		return new String(chars);
	}





	/** Converts an array of utf-8 bytes into a string. */
	static String utf8ToString(byte[] aMessage)
	{
		return utf8ToString(aMessage, 0, aMessage.length);
	}





	/** Converts an array of utf-8 bytes into a string. */
	static String utf8ToString(byte[] aMessage, int aOffset, int aLength)
	{
		try
		{
			return new String(aMessage, aOffset, aLength, "UTF-8");
		}
		catch (UnsupportedEncodingException exc)
		{
			Log.d(TAG, "Failed to convert UTF-8 to string", exc);
			return "<non-convertible>";
		}
	}





	static byte[] stringToUtf8(String aString)
	{
		try
		{
			return aString.getBytes("UTF-8");
		}
		catch (UnsupportedEncodingException exc)
		{
			Log.d(TAG, "Failed to convert string to UTF-8", exc);
			return new byte[0];
		}
	}





	/** Decodes the two bytes at the specified position in the array as a BE16 number. */
	public static short decodeBE16(byte[] aData, int aIndex)
	{
		return (short)(((aData[aIndex] << 8) & 0xff00) | (aData[aIndex + 1] & 0xff));
	}





	/** Decodes the four bytes at the specified position in the array as a BE32 number. */
	public static int decodeBE32(byte[] aData, int aIndex)
	{
		return
		(
			((aData[aIndex] << 24) & 0xff000000) |
			((aData[aIndex + 1] << 16) & 0xff0000) |
			((aData[aIndex + 2] << 8) & 0xff00) |
			(aData[aIndex + 3] & 0xff)
		);
	}




	/** Encodes the value as a two-byte BE16 number. */
	public static byte[] encodeBE16(short aValue)
	{
		return new byte[]
		{
			(byte)(aValue >> 8),
			(byte)(aValue & 0xff),
		};
	}
}
