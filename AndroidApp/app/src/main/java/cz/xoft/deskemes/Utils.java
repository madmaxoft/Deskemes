package cz.xoft.deskemes;





public class Utils
{
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
	public static String bytesToHex(byte[] aBytes)
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
}
