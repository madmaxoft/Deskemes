package cz.xoft.deskemes;

import android.app.ActivityManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.SocketAddress;
import java.util.HashSet;
import java.util.Set;





/** A service that runs in the background all the time and listens for broadcasted UDP beacons from Deskemes.
Tries to listen on the PRIMARY_PORT; if that is not available, tries ALTERNATE_PORT. */
public class UdpListenerService extends Service
{
	/** The Log tag for this class. */
	private static String TAG = "UdpListenerService";

	/** The UDP port on which the service listens primarily. */
	final static int PRIMARY_PORT = 24816;

	/** The UDP port on which the service listens if the primary port is not available. */
	final static int ALTERNATE_PORT = 4816;

	/** The thread processing the incoming packets. */
	Thread mThread;

	/** The socket used for listening. */
	DatagramSocket mSocket;

	/** Flag that is set to true to terminate the listening thread. */
	boolean mShouldTerminate = false;

	/** The addresses that are already connected.
	Used to quickly discard packets sent by the already-connected clients.
	This list is managed by TODO: external class via addConnectedAddress() and delConnectedAddress(). */
	Set<InetAddress> mConnectedAddresses = new HashSet<>();





	@Override
	public void onCreate()
	{
		mThread = new Thread(new Runnable()
		{
			public void run()
			{
				try
				{
					try
					{
						SocketAddress sa = new InetSocketAddress(PRIMARY_PORT);
						mSocket = new DatagramSocket(sa);
					}
					catch (Exception exc)
					{
						SocketAddress sa = new InetSocketAddress(ALTERNATE_PORT);
						mSocket = new DatagramSocket(sa);
					}
					Log.d(TAG, "Listening on " + mSocket.getLocalSocketAddress());
					mSocket.setBroadcast(true);
					while (!mShouldTerminate)
					{
						final int MAX_PACKET_LENGTH = 1000;
						byte[] packetData = new byte[MAX_PACKET_LENGTH];
						DatagramPacket packet = new DatagramPacket(packetData, packetData.length);
						mSocket.receive(packet);
						processPacket(packet);
					}
				}
				catch (Exception exc)
				{
					Log.d(TAG, "no longer listening for UDP broadcasts", exc);
				}
			}
		});
		mThread.start();
	}





	/** Processes the received UDP packet.
	Called by mThread when a UDP packet is received. */
	private void processPacket(DatagramPacket aPacket)
	{
		int length = aPacket.getLength();
		byte[] data = aPacket.getData();
		if (length < 31)
		{
			// Not long enough to be a Deskemes beacon
			return;
		}
		ByteArrayReader reader = new ByteArrayReader(data);
		if (!reader.checkAsciiString("Deskemes"))
		{
			return;
		}
		int protocolVersion;
		byte[] publicID;
		int tcpPort;
		boolean isDiscovery;
		try
		{
			protocolVersion = reader.readBE16();
			publicID = reader.readBE16LBlob();
			tcpPort = reader.readBE16();
			isDiscovery = reader.readBoolean();
		}
		catch (ByteArrayReader.DataEndReachedException exc)
		{
			return;
		}
		if (isCurrentlyConnected(aPacket.getAddress()) && !isDiscovery)
		{
			return;
		}
		Log.d(TAG, "Received a Deskemes beacon from " + aPacket.getSocketAddress() +
			" port " + tcpPort +
			", PublicID 0x" + Utils.bytesToHex(publicID) +
			", version " + protocolVersion
		);
		// TODO: Report the attempt so that a connection to the client can be made
	}





	@Override
	public void onDestroy()
	{
		Log.i(TAG, "Terminating the listener");
		mShouldTerminate = true;
		try
		{
			if (mSocket != null)
			{
				mSocket.close();
			}
			mThread.join();
		}
		catch (Exception exc)
		{
			Log.e(TAG, "Caught an exception: " + exc.getMessage());
		}
	}





	@Override
	public int onStartCommand(Intent intent, int flags, int startId)
	{
		/*
		mShouldRestartSocketListen = true;
		Log.i("UDP", "Service started");
		return START_STICKY;
		*/
		return START_STICKY;
	}





	@Override
	public IBinder onBind(Intent intent)
	{
		return null;
	}





	/** Returns true if the service is currently running, false otherwise. */
	public static boolean isRunning(Context aContext)
	{
		ActivityManager manager = (ActivityManager) aContext.getSystemService(Context.ACTIVITY_SERVICE);
        for (ActivityManager.RunningServiceInfo service : manager.getRunningServices(Integer.MAX_VALUE))
		{
			if (UdpListenerService.class.getName().equals(service.service.getClassName()))
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
		aContext.startService(new Intent(aContext, UdpListenerService.class));
	}




	/** Returns true if the specified address is in the list of currently connected clients. */
	synchronized public boolean isCurrentlyConnected(InetAddress aAddress)
	{
		return mConnectedAddresses.contains(aAddress);
	}




	/** Adds the specified address to the list of currently connected clients.
	From then on, UDP beacons from those clients will be ignored unless their IsDiscovery flag is on. */
	synchronized public void addConnectedAddress(InetAddress aAddress)
	{
		mConnectedAddresses.add(aAddress);
	}





	/** Adds the specified address to the list of currently connected clients.
	UDP beacons from this client will no longer be ignored. */
	synchronized public void delConnectedAddress(InetAddress aAddress)
	{
		mConnectedAddresses.remove(aAddress);
	}
}
