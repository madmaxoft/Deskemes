package cz.xoft.deskemes;

import org.junit.Test;

import static org.junit.Assert.*;





public class ByteArrayReaderTest
{
	@Test
	public void readHighNumbers() throws Exception
	{
		byte[] inputData = new byte[]
		{
			(byte)0xa0, (byte)0xa0,  // High BE16 number
			(byte)0xc0, (byte)0xc0, (byte) 0xc0, (byte) 0xc0,  // High BE32 number
		};
		ByteArrayReader bar = new ByteArrayReader(inputData);
		assertEquals(0xa0a0, bar.readBE16() & 0xffff);
		assertEquals(0xc0c0c0c0L, bar.readBE32() & 0xffffffffL);
	}
}
