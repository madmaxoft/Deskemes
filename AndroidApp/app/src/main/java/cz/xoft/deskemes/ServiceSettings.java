package cz.xoft.deskemes;

import android.content.Context;
import android.os.Build;
import android.text.TextUtils;
import android.util.Base64;
import android.util.Log;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Random;





/** Stores the user-settable values for the service.
The values are persisted in a single file. The file is a collection of "key" and "value" lines
interleaved so that each value is preceded by its key. */

public class ServiceSettings
{
	/** The tag used for logging. */
	private static final String TAG = "ServiceSettings";

	/** The friendly name that is shown by the remote peers for this device.
	 Settable by the user in the UI, defaults to device name. */
	private String mFriendlyName;

	/** The PublicID used to uniquely identify the device.
	 The remote peers match this with our certificate to provide persistence to user-accepted pairing. */
	private byte[] mPublicID;

	/** If true, the app connects to unknown clients who use Discovery beacons.
	Normally true after fresh install so that the initial connection can be made; user can set this to false
	once they discover all their clients. */
	private boolean mShouldRespondToDiscovery = true;

	/** The context used for reading and writing the config file. */
	private Context mContext;





	/** Loads the settings, or fills in the defaults if not set yet. */
	ServiceSettings(Context aContext)
	{
		mContext = aContext;
		load();
		fillMissingWithDefaults();
	}





	private void load()
	{
		try
		{
			FileInputStream fis = mContext.openFileInput("ServiceSettings.conf");
			BufferedReader br = new BufferedReader(new InputStreamReader(fis, "UTF-8"));
			while (true)
			{
				String key = br.readLine();
				String value = br.readLine();
				if ((key == null) || (value == null))
				{
					break;
				}
				switch (key)
				{
					case "FriendlyName":
					{
						mFriendlyName = value;
						break;
					}
					case "PublicID":
					{
						mPublicID = Base64.decode(value, Base64.NO_WRAP);
						break;
					}
					case "ShouldRespondToDiscovery":
					{
						mShouldRespondToDiscovery = ((value == "Y") || (value == "y") || (value == "1"));
						break;
					}
				}
			}
		}
		catch (IOException exc)
		{
		}
	}





	private void fillMissingWithDefaults()
	{
		boolean hasChanged = false;
		if (mFriendlyName == null)
		{
			mFriendlyName = getDeviceName();
			hasChanged = true;
		}
		if (mPublicID == null)
		{
			mPublicID = createPublicID();
			hasChanged = true;
		}
		if (hasChanged)
		{
			save();
		}
	}





	/** Creates the data for a public ID (but doesn't assign it, returns it instead.) */
	private byte[] createPublicID()
	{
		MessageDigest md;
		byte[] res;
		try
		{
			md = MessageDigest.getInstance("SHA-1");
			md.update(mFriendlyName.getBytes("UTF-8"));
			md.update(generateRandomBytes(20));
			res = md.digest();
		}
		catch (NoSuchAlgorithmException | UnsupportedEncodingException exc)
		{
			Log.d(TAG, "Failed to create PublicID hash, using pure-random instead", exc);
			res = generateRandomBytes(40);
		}
		return res;
	}





	private byte[] generateRandomBytes(int aNumBytes)
	{
		byte[] res = new byte[aNumBytes];
		(new Random()).nextBytes(res);
		return res;
	}





	/** Returns the consumer friendly device name */
	public static String getDeviceName()
	{
		String manufacturer = Build.MANUFACTURER;
		String model = Build.MODEL;
		if (model.startsWith(manufacturer))
		{
			return capitalize(model);
		}
		return capitalize(manufacturer) + " " + model;
	}





	private static String capitalize(String str)
	{
		if (TextUtils.isEmpty(str))
		{
			return str;
		}
		char[] arr = str.toCharArray();
		boolean capitalizeNext = true;
		String phrase = "";
		for (char c : arr)
		{
			if (capitalizeNext && Character.isLetter(c))
			{
				phrase += Character.toUpperCase(c);
				capitalizeNext = false;
				continue;
			}
			else if (Character.isWhitespace(c))
			{
				capitalizeNext = true;
			}
			phrase += c;
		}
		return phrase;
	}





	/** Returns the public ID. */
	byte[] getPublicID()
	{
		if (mPublicID == null)
		{
			Log.d(TAG, "PublicID is not assigned, creating new");
			mPublicID = createPublicID();
			save();
		}
		return mPublicID;
	}





	/** Returns the friendly name that is shown to the user by the remote peers, to represent this device. */
	String getFriendlyName()
	{
		if (mFriendlyName == null)
		{
			Log.d(TAG, "Friendly name is not assigned, using default");
			mFriendlyName = "noname";
			save();
		}
		return mFriendlyName;
	}





	/** Returns whether the app should respond to discovery beacons. */
	boolean shouldRespondToDiscovery() { return mShouldRespondToDiscovery; }





	/** Updates the friendly name. */
	void setFriendlyName(final String aFriendlyName)
	{
		mFriendlyName = aFriendlyName;
		save();
	}





	private void save()
	{
		try
		{
			FileOutputStream fos = mContext.openFileOutput("ServiceSettings.conf", 0);
			fos.write("FriendlyName\n".getBytes());
			fos.write(mFriendlyName.getBytes("UTF-8"));
			fos.write("\nPublicID\n".getBytes());
			fos.write(Base64.encode(mPublicID, Base64.NO_WRAP));
			fos.write("\nShouldRespondToDiscovery\n".getBytes());
			fos.write((mShouldRespondToDiscovery ? "1" : "0").getBytes());
			fos.close();
		}
		catch (IOException exc)
		{
			Log.d(TAG, "Failed to save my identity", exc);
		}
	}
}
