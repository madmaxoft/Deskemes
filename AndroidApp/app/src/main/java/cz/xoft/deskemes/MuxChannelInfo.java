package cz.xoft.deskemes;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.telephony.TelephonyManager;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.Arrays;





class MuxChannelInfo
	extends Connection.MuxChannel
{
	/** The tag used for logging. */
	private static final String TAG = "MuxChannelInfo";

	// Data types:
	private byte dtByte = 0x01;
	private byte dtShort = 0x02;
	private byte dtInt = 0x03;
	private byte dtLong = 0x04;
	private byte dtFixed32 = 0x05;
	private byte dtFixed64 = 0x06;
	private byte dtString = 0x07;  // BE16 length + UTF-8 string

	private static final String[] mKnownIDs =
	{
		"batl",
		"imei",
		"sigs",
		"time",
	};

	private String mImei = getImei();





	@SuppressLint ("MissingPermission")
	private String getImei()
	{
		try
		{
			Context ctx = ConnectivityService.getContext();
			TelephonyManager telephonyManager = (TelephonyManager)ctx.getSystemService(Context.TELEPHONY_SERVICE);
			return telephonyManager.getDeviceId();
		}
		catch (Exception exc)
		{
			return null;
		}
	}





	/** Creates a new instance bound to the specified connection. */
	MuxChannelInfo(Connection aConnection)
	{
		super(aConnection);
	}





	@Override
	void processMessage(byte[] aMessage)
	{
		ByteArrayOutputStream resp = new ByteArrayOutputStream();
		ByteArrayOutputStream workspace = new ByteArrayOutputStream();
		for (int i = 0; i < aMessage.length - 3; i += 4)
		{
			byte[] idMask = Arrays.copyOfRange(aMessage, i, i + 4);
			for (String id: mKnownIDs)
			{
				if (matchesMask(id, idMask))
				{
					byte[] value = serializeResponseValue(workspace, id);
					if (resp.size() + value.length > 65000)
					{
						sendMessage(resp.toByteArray());
						resp.reset();
					}
					try
					{
						resp.write(value);
					}
					catch (IOException exc)
					{
						Log.d(TAG, "Failed to write value " + id, exc);
					}
				}
			}
		}
		if (resp.size() > 0)
		{
			sendMessage(resp.toByteArray());
			resp.reset();
		}
	}





	/** Returns true if the ID matches the Mask.
	A match is defined either by exact-matching, or the mask can have ? characters to specify a wildcard character.  */
	private boolean matchesMask(String aID, byte[] aMask)
	{
		for (int i = 0; i < 4; ++i)
		{
			if ((aID.charAt(i) != aMask[i]) && (aMask[i] != '?'))
			{
				return false;
			}
		}
		return true;
	}





	/** Returns the value of the corresponding ID, serialized for response. */
	private byte[] serializeResponseValue(ByteArrayOutputStream aWorkspace, String aID)
	{
		aWorkspace.reset();
		aWorkspace.write(aID.charAt(0));
		aWorkspace.write(aID.charAt(1));
		aWorkspace.write(aID.charAt(2));
		aWorkspace.write(aID.charAt(3));

		// Synchronize the following switch with mKnownIDs[]
		switch (aID)
		{
			case "batl": return serializeShort(aWorkspace, getBatteryPercent());
			case "imei": return serializeString(aWorkspace, mImei);
			case "sigs": return serializeByte(aWorkspace, getSignalStrengthPercent());
			case "time": return serializeLong(aWorkspace, System.currentTimeMillis() / 1000);
		}
		throw new AssertionError("Unknown ID");
	}





	/** Serializes a Byte value into the byte array, using the pre-filled workspace.
	 Expects that the value ID has already been written to the workspace. */
	private byte[] serializeString(ByteArrayOutputStream aWorkspace, String aValue)
	{
		byte[] val = Utils.stringToUtf8(aValue);
		int len = val.length;
		aWorkspace.write(dtString);
		aWorkspace.write((len >> 8) & 0xff);
		aWorkspace.write(len & 0xff);
		try
		{
			aWorkspace.write(val);
		}
		catch (IOException exc)
		{
			Log.d(TAG, "Failed to serialize string value", exc);
		}
		return aWorkspace.toByteArray();
	}





	/** Serializes a Byte value into the byte array, using the pre-filled workspace.
	 Expects that the value ID has already been written to the workspace. */
	private byte[] serializeByte(ByteArrayOutputStream aWorkspace, byte aValue)
	{
		aWorkspace.write(dtByte);
		aWorkspace.write(aValue & 0xff);
		return aWorkspace.toByteArray();
	}





	/** Serializes a Short (BE16) value into the byte array, using the pre-filled workspace.
	 Expects that the value ID has already been written to the workspace. */
	private byte[] serializeShort(ByteArrayOutputStream aWorkspace, short aValue)
	{
		aWorkspace.write(dtShort);
		aWorkspace.write((aValue >> 8) & 0xff);
		aWorkspace.write(aValue & 0xff);
		return aWorkspace.toByteArray();
	}





	/** Serializes a Long (BE64) value into the byte array, using the pre-filled workspace.
	 Expects that the value ID has already been written to the workspace. */
	private byte[] serializeInt(ByteArrayOutputStream aWorkspace, int aValue)
	{
		aWorkspace.write(dtInt);
		aWorkspace.write((aValue >> 24) & 0xff);
		aWorkspace.write((aValue >> 16) & 0xff);
		aWorkspace.write((aValue >> 8) & 0xff);
		aWorkspace.write(aValue & 0xff);
		return aWorkspace.toByteArray();
	}





	/** Serializes a Long (BE64) value into the byte array, using the pre-filled workspace.
	Expects that the value ID has already been written to the workspace. */
	private byte[] serializeLong(ByteArrayOutputStream aWorkspace, long aValue)
	{
		aWorkspace.write(dtLong);
		aWorkspace.write((int)((aValue >> 56) & 0xff));
		aWorkspace.write((int)((aValue >> 48) & 0xff));
		aWorkspace.write((int)((aValue >> 40) & 0xff));
		aWorkspace.write((int)((aValue >> 32) & 0xff));
		aWorkspace.write((int)((aValue >> 24) & 0xff));
		aWorkspace.write((int)((aValue >> 16) & 0xff));
		aWorkspace.write((int)((aValue >> 8) & 0xff));
		aWorkspace.write((int)(aValue & 0xff));
		return aWorkspace.toByteArray();
	}





	/** Returns the signal strength as percentage, or -1 if not available. */
	private byte getSignalStrengthPercent()
	{
		// TODO
		return -1;
	}





	/** Returns the battery charge, as percent. */
	private short getBatteryPercent()
	{
		try
		{
			IntentFilter ifilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
			Intent batteryStatus = ConnectivityService.getContext().registerReceiver(null, ifilter);
			if (batteryStatus == null)
			{
				Log.d(TAG, "Failed to query battery status");
				return -1;
			}
			int level = batteryStatus.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
			int scale = batteryStatus.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
			Log.d(TAG, "Battery status: " + level + " / " + scale + " (" + (short)((float)level / (float)scale * 100) + " %)");
			return (short)((float)level / (float)scale * 100);
		}
		catch (Exception exc)
		{
			Log.d(TAG, "Failed to get battery status", exc);
			return -1;
		}
	}





	@Override
	void initialize(byte[] aServiceInitData) throws ServiceInitFailedException, ServiceInitNoPermissionException
	{
		// No initialization needed
	}
}
