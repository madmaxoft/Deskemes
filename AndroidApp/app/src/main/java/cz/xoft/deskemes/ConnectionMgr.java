package cz.xoft.deskemes;

import android.util.Log;

import java.net.InetAddress;
import java.net.InetSocketAddress;





/** The manager of all TCP connections to all desktop clients.
Receives beacons from the UDP listener and decides whether to connect to the remotes.
Opens and drives the TCP connections. */
public class ConnectionMgr
	extends Thread
	implements UdpListenerThread.IBeaconNotificationConsumer
{
	/** The tag used for logging. */
	private static String TAG = "ConnectionMgr";





	/** Interface for address-based blacklists.
	Functions in this interface are called to block or allow certain addresses from connecting.
	Used by the AddressBlacklistBeaconFilter to receive the blacklist data. */
	interface IAddressBlacklist
	{
		/** Adds the address to blacklist. */
		public void blockAddress(InetAddress aAddress);

		/** Removes the address from the blacklist. */
		public void allowAddress(InetAddress aAddress);
	}





	// IBeaconNotificationConsumer override:
	@Override
	public void beaconReceived(InetSocketAddress aAddressToConnect, int aProtocolVersion, byte[] aPublicID, boolean aIsDiscovery)
	{
		Log.d(TAG, "Received a Deskemes beacon for " + aAddressToConnect +
			", PublicID 0x" + Utils.bytesToHex(aPublicID) +
			", version " + aProtocolVersion
		);
		// TODO
	}





	@Override
	public void run()
	{
		// TODO
	}
}
