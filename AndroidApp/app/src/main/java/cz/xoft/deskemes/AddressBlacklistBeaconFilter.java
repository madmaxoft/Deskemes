package cz.xoft.deskemes;

import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.util.HashSet;
import java.util.Set;





/** A filter-consumer that will only allow notifications through if their address is not on the internal blacklist. */
public class AddressBlacklistBeaconFilter
	implements UdpListenerThread.IBeaconNotificationConsumer, ConnectionMgr.IAddressBlacklist
{
	/** The consumer to which the notifications are sent if they are allowed to pass. */
	UdpListenerThread.IBeaconNotificationConsumer mNextConsumer;

	/** The blacklisted addresses from which beacons are to be ignored. */
	private Set<InetAddress> mBlacklistedAddresses = new HashSet<>();





	/** Creates a new instance of the filter that sends the allowed notifications to aNextConsumer. */
	AddressBlacklistBeaconFilter(UdpListenerThread.IBeaconNotificationConsumer aNextConsumer)
	{
		assert(aNextConsumer != null);

		mNextConsumer = aNextConsumer;
	}





	// IBeaconNotificationConsumer override:
	@Override
	public void beaconReceived(InetSocketAddress aAddressToConnect, int aProtocolVersion, byte[] aPublicID, boolean aIsDiscovery)
	{
		if (isBlacklisted(aAddressToConnect.getAddress()))
		{
			return;
		}
		mNextConsumer.beaconReceived(aAddressToConnect, aProtocolVersion, aPublicID, aIsDiscovery);
	}





	/** Returns true if the specified address is in the list of currently connected clients. */
	synchronized public boolean isBlacklisted(InetAddress aAddress)
	{
		return mBlacklistedAddresses.contains(aAddress);
	}





	/** Adds the specified address to the blacklist. */
	synchronized public void blockAddress(InetAddress aAddress)
	{
		mBlacklistedAddresses.add(aAddress);
	}





	/** Removes the specified address from the blacklist. */
	synchronized public void allowAddress(InetAddress aAddress)
	{
		mBlacklistedAddresses.remove(aAddress);
	}
}
