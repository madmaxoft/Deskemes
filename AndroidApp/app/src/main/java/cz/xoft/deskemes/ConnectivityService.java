package cz.xoft.deskemes;

import android.app.ActivityManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.util.Log;





public class ConnectivityService extends Service
{
	/** A simple Binder that allows access to the underlying service directly. */
	public class Binder extends android.os.Binder
	{
		ConnectivityService mService;

		Binder(ConnectivityService aService)
		{
			mService = aService;
		}

		public ConnectivityService service()
		{
			return mService;
		}
	}





	/** The tag used for logging. */
	private static String TAG = "ConnectivityService";

	/** The thread that listens for and reports valid Deskemes UDP beacons. */
	private UdpListenerThread mUdpListenerThread;

	/** The filter that implements the blacklist for ignoring UDP beacons based on their address. */
	private AddressBlacklistBeaconFilter mAddressBlacklistBeaconFilter;

	/** The manager of all TCP connections. */
	private ConnectionMgr mConnectionMgr;

	/** The settings for the service. */
	private ServiceSettings mSettings;

	/** The manager of the approved peers. */
	private ApprovedPeers mApprovedPeers;

	/** The Binder used to access the service through binding. */
	private IBinder mBinder = new Binder(this);





	/** Creates (the only) instance of this service.
	Called by the OS when the service is started. */
	public ConnectivityService()
	{
	}





	/** Returns the connection manager used within the service. */
	ConnectionMgr connectionMgr()
	{
		return mConnectionMgr;
	}





	/** Returns the approved peers manager used within the service. */
	ApprovedPeers approvedPeers()
	{
		return mApprovedPeers;
	}





	/** Returns the ServiceSettings used within the service. */
	public ServiceSettings settings()
	{
		return mSettings;
	}





	@Override
	public int onStartCommand(Intent intent, int flags, int startId)
	{
		return START_STICKY;
	}





	@Override
	public void onCreate()
	{
		mSettings = new ServiceSettings(this);
		mApprovedPeers = new ApprovedPeers(this);
		mUdpListenerThread = new UdpListenerThread();
		mConnectionMgr = new ConnectionMgr(this, mApprovedPeers, mSettings);
		mAddressBlacklistBeaconFilter = new AddressBlacklistBeaconFilter(mConnectionMgr);
		mUdpListenerThread.setBeaconNotificationConsumer(mAddressBlacklistBeaconFilter);
		mConnectionMgr.setAddressBlacklist(mAddressBlacklistBeaconFilter);
		mConnectionMgr.start();
		mUdpListenerThread.start();
	}





	@Override
	public void onDestroy()
	{
		// Terminate the UDP listener:
		if (mUdpListenerThread != null)
		{
			Log.i(TAG, "Terminating the listener");
			mUdpListenerThread.terminate();
			try
			{
				mUdpListenerThread.join();
			}
			catch (Exception exc)
			{
				Log.e(TAG, "Failed to join UdpListenerThread", exc);
			}
			mUdpListenerThread = null;
		}

		// Terminate the ConnectionMgr:
		if (mConnectionMgr != null)
		{
			Log.i(TAG, "Terminating the connections");
			mConnectionMgr.terminate();
			try
			{
				mConnectionMgr.join();
			}
			catch (Exception exc)
			{
				Log.e(TAG, "Failed to join ConnectionMgr", exc);
			}
			mConnectionMgr = null;
		}

		mAddressBlacklistBeaconFilter = null;
	}





	/** Returns true if the service is currently running, false otherwise. */
	public static boolean isRunning(Context aContext)
	{
		ActivityManager manager = (ActivityManager) aContext.getSystemService(Context.ACTIVITY_SERVICE);
		for (ActivityManager.RunningServiceInfo service : manager.getRunningServices(Integer.MAX_VALUE))
		{
			if (ConnectivityService.class.getName().equals(service.service.getClassName()))
			{
				return true;
			}
		}
		return false;
	}





	/** Starts the service if it is not running, otherwise does nothing. */
	public static void startIfNotRunning(Context aContext)
	{
		if (isRunning(aContext))
		{
			return;
		}
		Log.i(TAG, "Starting service");
		aContext.startService(new Intent(aContext, ConnectivityService.class));
	}





	@Override
	public IBinder onBind(Intent intent)
	{
		return mBinder;
	}
}
