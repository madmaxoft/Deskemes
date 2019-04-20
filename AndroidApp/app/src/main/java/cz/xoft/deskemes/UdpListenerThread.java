package cz.xoft.deskemes;

import android.util.Log;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.SocketAddress;
import java.util.HashSet;
import java.util.Set;





/** A thread that runs in the background all the time (through ConnectivityService) and listens
for broadcasted UDP beacons from Deskemes.
Tries to listen on the PRIMARY_PORT; if that is not available, tries ALTERNATE_PORT. */
public class UdpListenerThread extends Thread
{
	/** The interface that is used to report the received UDP beacons to. */
	public interface IBeaconNotificationConsumer
	{
		/** The function called when a UDP beacon is received. */
		public void beaconReceived(InetSocketAddress aAddressToConnect, int aProtocolVersion, byte[] aPublicID, boolean aIsDiscovery);
	};






	/** The tag used for logging. */
	private static String TAG = "UdpListenerThread";

	/** The UDP port on which the service listens primarily. */
	private final static int PRIMARY_PORT = 24816;

	/** The UDP port on which the service listens if the primary port is not available. */
	private final static int ALTERNATE_PORT = 4816;

	/** The UDP socket used for listening. */
	private DatagramSocket mSocket;

	/** Flag that is set to true to terminate the listening thread. */
	private boolean mShouldTerminate = false;

	/** The destination consumer that is notified whenever a valid UDP beacon is received. */
	private IBeaconNotificationConsumer mBeaconNotificationConsumer = null;





	/** Updates the beacon notification consumer used by this class. */
	public void setBeaconNotificationConsumer(IBeaconNotificationConsumer aBeaconNotificationConsumer)
	{
		mBeaconNotificationConsumer = aBeaconNotificationConsumer;
	}





	@Override
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





	/** Processes the received UDP packet.
	Called by mThread when a UDP packet is received. */
	private void processPacket(DatagramPacket aPacket)
	{
		if (mBeaconNotificationConsumer == null)
		{
			// No-where to report the beacon anyway, so bail out immediately:
			return;
		}
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
		InetSocketAddress addr = new InetSocketAddress(aPacket.getAddress(), tcpPort);
		mBeaconNotificationConsumer.beaconReceived(addr, protocolVersion, publicID, isDiscovery);
	}





	/** Asks the thread to terminate. */
	public void terminate()
	{
		mShouldTerminate = true;
		if (mSocket != null)
		{
			mSocket.close();
		}
	}
}
