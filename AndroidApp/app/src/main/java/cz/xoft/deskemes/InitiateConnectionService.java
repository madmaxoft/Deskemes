package cz.xoft.deskemes;

import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.AsyncTask;
import android.os.IBinder;
import android.support.annotation.Nullable;





/** Mini-service that is invoked using the ADB AM shell command to connect to specific TCP addresses
 This is used together with ADB port-forwarding to connect through USB. */
public class InitiateConnectionService extends Service
{
	private boolean mIsBound;

	/** The port on which the remote is listening (on all given host addresses). */
	private int mPort;

	/** The IP addresses on which the remote is listening (to which the connection should be tried). */
	private String[] mAddresses;





	@Override
	public void onStart(Intent intent, int startId)
	{
		super.onStart(intent, startId);

		// Get the addresses to connect to:
		mPort = intent.getIntExtra("Port", -1);
		if (mPort < 0)
		{
			stopSelf();
			return;
		}
		String addresses = intent.getStringExtra("Addresses");
		if (addresses == null)
		{
			stopSelf();
			return;
		}
		mAddresses = addresses.split(" ");
		if ((mAddresses == null) || (mAddresses.length == 0))
		{
			// Invalid list of addresses
			stopSelf();
			return;
		}

		// Bind to ConnectivityService:
		Intent svcIntent = new Intent(this, ConnectivityService.class);
		mIsBound = bindService(svcIntent, mSvcConn, Context.BIND_AUTO_CREATE);
	}





	@Override
	public void onDestroy()
	{
		if (mIsBound)
		{
			unbindService(mSvcConn);
			mIsBound = false;
		}
		super.onDestroy();
	}





	@Nullable
	@Override
	public IBinder onBind(Intent intent)
	{
		return null;
	}





	/** Async task that connects to the specified local port. */
	private class ConnectAsyncTask extends AsyncTask<Integer, Integer, Integer>
	{
		private final ConnectivityService mService;





		ConnectAsyncTask(ConnectivityService aService)
		{
			mService = aService;
		}





		@Override
		protected Integer doInBackground(Integer... aParams)
		{
			for (String addr: mAddresses)
			{
				if (!addr.isEmpty())
				{
					mService.connectionMgr().connectTo(addr, mPort);
				}
			}
			return null;
		}





		@Override
		protected void onPostExecute(Integer aResult)
		{
			// Finish the activity
			stopSelf();
		}
	}





	/** Connection to the ConnectivityService. */
	private ServiceConnection mSvcConn = new ServiceConnection()
	{
		@Override
		public void onServiceConnected(ComponentName aName, IBinder aBinder)
		{
			ConnectivityService service = ((ConnectivityService.Binder)aBinder).service();
			new ConnectAsyncTask(service).execute();
		}





		@Override
		public void onServiceDisconnected(ComponentName name)
		{
			// Nothing needed
		}
	};
}
