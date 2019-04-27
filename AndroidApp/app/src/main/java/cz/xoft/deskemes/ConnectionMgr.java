package cz.xoft.deskemes;

import android.util.Log;

import java.io.IOException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.nio.channels.CancelledKeyException;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.SocketChannel;
import java.util.ArrayList;
import java.util.List;
import java.util.Vector;





/** The manager of all TCP connections to all desktop clients.
Receives beacons from the UDP listener and decides whether to connect to the remotes.
Opens and drives the TCP connections. */
public class ConnectionMgr
	extends Thread
	implements UdpListenerThread.IBeaconNotificationConsumer
{
	/** Interface for address-based blacklists.
	Functions in this interface are called to block or allow certain addresses from connecting.
	Used by the AddressBlacklistBeaconFilter to receive the blacklist data. */
	interface IAddressBlacklist
	{
		/** Adds the address to blacklist. */
		void blockAddress(InetAddress aAddress);

		/** Removes the address from the blacklist. */
		void allowAddress(InetAddress aAddress);
	}





	/** The tag used for logging. */
	private static String TAG = "ConnectionMgr";

	/** The Selector used to drive all the TCP sockets' channels. */
	private Selector mSelector;

	/** All the TCP sockets' channels. */
	private Vector<Connection> mConnections = new Vector<>();

	/** The blacklist used to block beacons from hosts that are already connected in mConnections[]. */
	private IAddressBlacklist mAddressBlacklist;

	/** If set to true, the thread will terminate. */
	private boolean mShouldTerminate = false;

	/** The connections that are to be registered once mSelector returns from its select() call. */
	private final List<Connection> mRegistrationQueue = new Vector<>();





	// IBeaconNotificationConsumer override:
	@Override
	public void beaconReceived(InetSocketAddress aAddressToConnect, int aProtocolVersion, byte[] aPublicID, boolean aIsDiscovery)
	{
		Log.d(TAG, "Received a Deskemes beacon for " + aAddressToConnect +
			", PublicID 0x" + Utils.bytesToHex(aPublicID) +
			", version " + aProtocolVersion
		);
		// TODO: Do not connect to unknown addresses / IDs unless their IsDiscovery flag is set.
		queueRegisterConnection(new Connection(aAddressToConnect, aPublicID));
		mAddressBlacklist.blockAddress(aAddressToConnect.getAddress());
	}





	/** Adds the specified connection to the queue of connections to be registered in mSelector. */
	private void queueRegisterConnection(Connection c)
	{
		synchronized (mRegistrationQueue)
		{
			mRegistrationQueue.add(c);
		}
		mSelector.wakeup();
	}





	@Override
	public void run()
	{
		try
		{
			mSelector = Selector.open();
		}
		catch (IOException exc)
		{
			Log.d(TAG, "Cannot open selector", exc);
		}
		try
		{
			while (!mShouldTerminate)
			{
				// Add any pending connections to mSelector:
				synchronized (mRegistrationQueue)
				{
					for (Connection c: mRegistrationQueue)
					{
						c.register(mSelector);
					}
					mRegistrationQueue.clear();
				}

				// Wait for any event on the connections:
				mSelector.select();

				// Process the event:
				for (SelectionKey key: mSelector.selectedKeys())
				{
					if (!key.isValid())
					{
						continue;
					}
					try
					{
						if (key.isConnectable())
						{
							keyConnect(key);
						}
						if (key.isReadable())
						{
							keyRead(key);
						}
						if (key.isWritable())
						{
							keyWrite(key);
						}
					}
					catch (CancelledKeyException exc)
					{
						// The connection was closed mid-processing, continue with the next
					}
				}
			}
		}
		catch (IOException exc)
		{
			Log.d(TAG, "Failed to handle TCP connections", exc);
		}
	}





	/** Reads from the specified selected key / channel. */
	private void keyRead(SelectionKey aKey)
	{
		Connection c = (Connection)aKey.attachment();
		try
		{
			c.selectedRead();
		}
		catch (IOException exc)
		{
			Log.d(TAG, "Failed to read data", exc);
			try
			{
				c.channel().close();
			}
			catch (IOException exc2)
			{
				Log.d(TAG, "Failed to close channel after read failure", exc2);
			}
		}
	}





	/** Reads from the specified selected key / channel. */
	private void keyWrite(SelectionKey aKey)
	{
		Connection c = (Connection) aKey.attachment();
		c.selectedWrite();
	}





	/** Finishes the connection on the specified selected key / channel. */
	private void keyConnect(SelectionKey aKey)
	{
		SocketChannel ch = (SocketChannel) aKey.channel();
		try
		{
			if (ch.isConnectionPending())
			{
				ch.finishConnect();
			}
			ch.configureBlocking(false);
		}
		catch (IOException exc)
		{
			Log.d(TAG, "Failed to finish connection", exc);
		}
		aKey.interestOps(SelectionKey.OP_READ);
	}





	/** Signals to the thread that it should terminate. */
	void terminate()
	{
		mShouldTerminate = true;
		mSelector.wakeup();
	}




	/** Sets the specified object as the address blacklist handler.
	Addresses are then sent to this handler when they are to be blocked or allowed. */
	void setAddressBlacklist(IAddressBlacklist aBlacklist)
	{
		mAddressBlacklist = aBlacklist;
	}
}
