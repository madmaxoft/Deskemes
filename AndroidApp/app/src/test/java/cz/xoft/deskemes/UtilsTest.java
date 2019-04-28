package cz.xoft.deskemes;

import org.junit.Test;

import static org.junit.Assert.*;





public class UtilsTest
{
	@Test
	public void testStringFromBE32()
	{
		assertEquals("AAAA", Utils.stringFromBE32(0x41414141));
		assertEquals("aaaa", Utils.stringFromBE32(0x61616161));
		assertEquals("open", Utils.stringFromBE32(0x6f70656e));
		assertEquals("clse", Utils.stringFromBE32(0x636c7365));
		assertEquals("ping", Utils.stringFromBE32(0x70696e67));
	}





	@Test
	public void testUtf8ToString()
	{
		assertEquals("aa", Utils.utf8ToString(new byte[]{0x61, 0x61}));
		assertEquals("open", Utils.utf8ToString(new byte[]{0x61, 0x6f, 0x70, 0x65, 0x6e, 0x61}, 1, 4));
		assertEquals("ěščř", Utils.utf8ToString(Utils.hexStringToByteArray("c49bc5a1c48dc599")));
	}





	@Test
	public void testHexStringToByteArray()
	{
		assertArrayEquals("Failed 1", new byte[]{0x01, 0x02, 0x03, 0x04}, Utils.hexStringToByteArray("01020304"));
		assertArrayEquals("Failed 2", new byte[]{0x78, (byte)0x9a, 0x12}, Utils.hexStringToByteArray("789a12"));
	}
}
