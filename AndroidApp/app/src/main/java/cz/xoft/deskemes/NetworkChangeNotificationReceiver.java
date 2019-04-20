package cz.xoft.deskemes;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;





public class NetworkChangeNotificationReceiver extends BroadcastReceiver
{

	/** The network configuration has changed. */
	@Override
	public void onReceive(Context aContext, Intent aIntent)
	{
		Log.d("Deskemes", "NetworkChangeNotificationReceiver received");
		UdpListenerService.startIfNotRunning(aContext);
	}
}
