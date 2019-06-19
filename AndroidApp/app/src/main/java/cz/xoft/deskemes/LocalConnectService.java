package cz.xoft.deskemes;

import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.AsyncTask;
import android.os.IBinder;
import android.support.annotation.Nullable;





/** Mini-service that is invoked using the ADB AM shell command to connect to localhost
This is used together with ADB port-forwarding to connect through USB. */
public class LocalConnectService extends Service
{
	private int mLocalPort;
	private boolean mIsBound = false;





	@Override
	public void onStart(Intent intent, int startId)
	{
		super.onStart(intent, startId);

		// Get the port number:
		mLocalPort = intent.getIntExtra("LocalPort", -1);
		if ((mLocalPort <= 0) || (mLocalPort > 65535))
		{
			// Invalid port number
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
			mService.connectionMgr().connectToLocal(mLocalPort);
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
