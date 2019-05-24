package cz.xoft.deskemes;


import android.util.Log;

import java.io.IOException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.nio.BufferOverflowException;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.SocketChannel;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;





/** A single TCP connection and its protocol-parsing machinery.
Usage:
	1. Create an instance with the remote address (will not connect yet)
	2. In the Selector's thread, call register() (will start connecting)
	3. When selected, call selectedConnect(), selectedRead() or selectedWrite(), respectively

The cleartext packet composition works by employing a PacketBuilder over a ByteBuffer, twice:
	1. The packet payload is built in mOutgoingPacket using the mPacketBuilder
	2. The sendCleartextMessage() is used to send the packet
	3. The sendCleartextMessage internally uses the mOutgoingDataPacketBuilder to compose the raw packet into mOutgoingData
	4. The selector is notified that the channel is interested in writing data.
	5. The selectedWrite() function writes any date left over in mOutgoingData to the channel.
*/
class Connection
{
	/** Interface to be implemented by individual mux-protocol channels.
	Has some basic functionality, therefore not a pure interface, but rather a class. */
	abstract static class MuxChannel
	{
		/** The connection on which the channel lives. */
		final Connection mConnection;

		/** The ID of this channel in the Connection. */
		short mChannelID;


		/** Exception that is thrown if a MuxChannel fails to open the service. */
		class ServiceInitFailedException extends Exception
		{
			ServiceInitFailedException(String aMessage)
			{
				super(aMessage);
			}
		}

		/** Exception that is thrown if a MuxChannel fails to open the service due to missing (Android) permissions.
		The exception message is mis-used to store the missing permission's identifier. */
		class ServiceInitNoPermissionException extends Exception
		{
			ServiceInitNoPermissionException(String aMissingPermission)
			{
				super(aMissingPermission);
			}
		}





		/** Creates a new instance bound to the specified connection. */
		MuxChannel(Connection aConnection)
		{
			mConnection = aConnection;
		}





		/** Called by the connection when a message is received for the channel. */
		abstract void processMessage(byte[] aMessage);





		/** Called by the muxer when a new channel is requested; passes the remote-specified init data. */
		abstract void initialize(byte[] aServiceInitData) throws ServiceInitFailedException, ServiceInitNoPermissionException;

		/** Sends the specified message to the remote. */
		void sendMessage(byte[] aMessage)
		{
			mConnection.sendMuxMessage(mChannelID, aMessage);
		}
	}




	/** Exception that is thrown if there is no available ChannelID for a new channel registration. */
	class NoAvailableChannelIDException extends Exception
	{
	}





	private enum State
	{
		Initial,
		Encrypted,
	}


	// The individual cleartext message types (translated from ASCII strings to ints):
	private final static int msg_dsms = 0x64736d73;
	private final static int msg_fnam = 0x666e616d;
	private final static int msg_avtr = 0x61767472;
	private final static int msg_pubi = 0x70756269;
	private final static int msg_pubk = 0x7075626b;
	private final static int msg_stls = 0x73746c73;




	/** The tag used for logging. */
	private final static String TAG = "Connection";

	/** The NIO channel used for the connection. */
	private SocketChannel mChannel;

	/** The size of the buffer for incoming data. */
	private final static int MAX_INCOMING_DATA = 66000;  // Slightly more than 64 KiB

	/** The size of the buffer for the outgoing data. */
	private final static int MAX_OUTGOING_DATA = 66000;

	/** The buffer for the incoming data.
	By default it is in drain-mode, only during selectedRead() it is flipped to fill-mode for the channel read. */
	private ByteBuffer mIncomingData;

	/** The buffer for the outgoing data.
	By default it is in the fill-mode, only in selectedWrite() it is flipped to drain-mode for the channel write. */
	private ByteBuffer mOutgoingData;

	/** The PacketBuilder used by sendCleartextMessage to compose the raw packet in mOutgoingData from mOutgoingPacket. */
	private PacketBuilder mOutgoingDataPacketBuilder;

	/** Temporary buffer for building a single packet. */
	private ByteBuffer mOutgoingPacket;

	/** The PacketBuilder that builds packets inside mOutgoingPacket. */
	private PacketBuilder mPacketBuilder;

	/** The connection state */
	private State mState = State.Initial;

	/** The remote address to which the connection will be made in the register() call. */
	private final InetSocketAddress mRemoteAddress;

	/** The selection key assigned by the selector in the register() call. */
	private SelectionKey mSelectionKey;

	/** Set to true once the `dsms` packet is received.
	Used to check that the `dsms` packet is the first one received; if not, the connection is aborted. */
	private boolean mHasReceivedDsms;

	/** The PublicID presented by the remote in the `pubi` cleartext message. */
	private byte[] mRemotePublicID;

	/** The PublicID presented in the UDP beacon.
	Will be checked against the `pubi` cleartext message. */
	private final byte[] mBeaconPublicID;

	/** The PublicKey presented by the remote in the `pubk` cleartext message. */
	private byte[] mRemotePublicKey;

	/** All the currently opened mux-protocol channels, indexed by their ChannelID. */
	private Map<Short, MuxChannel> mMuxChannels;

	/** The ConnectionMgr that takes care of this connection. */
	private ConnectionMgr mConnMgr;





	/** Creates a new instance that will connect to aRemoteAddress, and expect to find aBeaconPublicID there. */
	Connection(ConnectionMgr aConnMgr, InetSocketAddress aRemoteAddress, byte[] aBeaconPublicID)
	{
		mConnMgr = aConnMgr;
		mRemoteAddress = aRemoteAddress;
		mBeaconPublicID = aBeaconPublicID;
		mIncomingData = ByteBuffer.allocate(MAX_INCOMING_DATA);
		mIncomingData.order(ByteOrder.BIG_ENDIAN);
		mOutgoingData = ByteBuffer.allocate(MAX_OUTGOING_DATA);
		mOutgoingData.order(ByteOrder.BIG_ENDIAN);
		mOutgoingPacket = ByteBuffer.allocate(1000);
		mOutgoingPacket.order(ByteOrder.BIG_ENDIAN);
		mPacketBuilder = new PacketBuilder(mOutgoingPacket);
		mOutgoingDataPacketBuilder = new PacketBuilder(mOutgoingData);
		mMuxChannels = new HashMap<>();
	}





	/** Creates the SocketChannel and registers it within the specified Selector. */
	void register(Selector aSelector) throws IOException
	{
		Log.d(TAG, "Registering connection in a Selector");
		mChannel = SocketChannel.open();
		mChannel.configureBlocking(false);
		mChannel.connect(mRemoteAddress);
		mSelectionKey = mChannel.register(aSelector, SelectionKey.OP_CONNECT, this);
		Log.d(TAG, "Registration done");
	}





	/** Reads the data from mChannel into mIncomingData; parses the protocol on the incoming data. */
	void selectedRead() throws IOException
	{
		assert(mChannel != null);
		assert(mIncomingData.limit() == MAX_INCOMING_DATA);  // The buffer is in fill-mode

		// Read and process the data:
		if (mChannel.read(mIncomingData) < 0)
		{
			close();
			return;
		}
		mIncomingData.flip();  // fill-mode -> read-mode
		while (true)
		{
			if (!extractAndProcessIncomingData())
			{
				break;
			}
		}
		mIncomingData.compact();  // Also flips read-mode -> fill-mode

		// If there is too much data left in the buffer, drop the connection:
		if (mIncomingData.limit() == 0)
		{
			Log.i(TAG, "Closing channel, it has filled the incoming data buffer with too much non-parsable data");
			try
			{
				mChannel.close();
			}
			catch (IOException exc)
			{
				Log.i(TAG, "Failed to close channel", exc);
			}
		}
	}





	/** Write data from mOutgoingData to mChannel. */
	void selectedWrite()
	{
		// Log.d(TAG, "Writing data");
		try
		{
			mOutgoingData.flip();
			mChannel.write(mOutgoingData);
		}
		catch (IOException exc)
		{
			close();
		}
		mOutgoingData.compact();
		if (mOutgoingData.position() > 0)
		{
			// Still needs more writes
			mSelectionKey.interestOps(SelectionKey.OP_READ | SelectionKey.OP_WRITE);
		}
		else
		{
			// Written all we had, only interested in reads now
			mSelectionKey.interestOps(SelectionKey.OP_READ);
		}
	}





	/** Attempts to process the data in mIncomingData buffer.
	Returns true if any data was extracted, false if there is not enough data for a complete protocol message. */
	private boolean extractAndProcessIncomingData()
	{
		switch (mState)
		{
			case Initial:
			{
				return extractAndHandleCleartextMessage();
			}
			case Encrypted:
			{
				// TODO: TLS and extract+process
				return extractAndHandleMuxMessage();
			}
		}
		return false;
	}





	/** Attempts to process a single cleartext protocol message out of mIncomingData.
	Returns true if a message was extracted, false if not. */
	private boolean extractAndHandleCleartextMessage()
	{
		try
		{
			int len = mIncomingData.limit();
			if (len < 6)
			{
				return false;
			}
			int msgType = mIncomingData.getInt();
			int msgLen = mIncomingData.getShort();
			if (msgLen < 0)
			{
				msgLen = 65536 + msgLen;
			}
			if (len < msgLen + 6)
			{
				return false;
			}
			// A full message is present, process it:
			int pos = mIncomingData.position();
			byte[] msgData = Arrays.copyOfRange(mIncomingData.array(), pos, pos + msgLen);
			mIncomingData.position(pos + msgLen);
			handleCleartextMessage(msgType, msgData);
		}
		catch (BufferUnderflowException exc)
		{
			return false;
		}
		return true;
	}





	/** Handles the specified cleartext message. */
	private void handleCleartextMessage(int aMsgType, byte[] aMsgData)
	{
		Log.d(TAG, "Received a cleartext message: `" + Utils.stringFromBE32(aMsgType) + "`; datalength: " + aMsgData.length);
		if ((aMsgType != msg_dsms) && !mHasReceivedDsms)
		{
			Log.d(TAG, "Didn't receive the `dsms` message as the first one, aborting connection.");
			close();
			return;
		}

		switch (aMsgType)
		{
			case msg_dsms: handleDsmsMessage(aMsgData); break;
			case msg_pubi: handlePubiMessage(aMsgData); break;
			case msg_pubk: handlePubkMessage(aMsgData); break;
			case msg_stls: handleStlsMessage(aMsgData); break;

			case msg_fnam: // handleFnamMessage(aMsgData); break;
			case msg_avtr: // handleAvtrMessage(aMsgData); break;
				// TODO: dummy
				Log.d(TAG, "Ignoring message");
				break;
			default:
			{
				Log.d(TAG, "Unknown message type, aborting connection.");
				close();
				break;
			}
		}
	}





	/** Handles the `pubi` cleartext protocol message.
	Verifies that the public ID is the same as received in the beacon.
	TODO: Checks with the known remotes list to see if we should pair immediately. */
	private void handlePubiMessage(byte[] aMsgData)
	{
		mRemotePublicID = aMsgData;
		if (!Arrays.equals(aMsgData, mBeaconPublicID))
		{
			close();
			return;
		}
		// TODO: Check the known remotes list
		mOutgoingPacket.put(Utils.stringToUtf8("DummyPublicKey"));
		sendCleartextMessage("pubk");
	}





	/** Handles the `pubk` cleartext protocol message.
	Checks that we have already received the remote's PublicID and we haven't received
	its PublicKey yet. Stores the Public key for later use in encryption. */
	private void handlePubkMessage(byte[] aMsgData)
	{
		if ((mRemotePublicID == null) || (mRemotePublicKey != null))
		{
			close();
			return;
		}
		mRemotePublicKey = aMsgData;
		// TODO: Check if we even want to connect to this device.
		// If yes, generate our own public key and send it
		sendCleartextMessage("stls");
	}





	/** Handles the `tsls` cleartext protocol message.
	Checks that we have everything needed for TLS, then switches to the TLS Mux protocol. */
	private void handleStlsMessage(byte[] aMsgData)
	{
		if ((mRemotePublicID == null) || (mRemotePublicKey == null))
		{
			Log.d(TAG, "Trying to start TLS without PubID / PubKey, closing connection.");
			close();
			return;
		}
		Log.d(TAG, "Starting TLS, msgData length = " + aMsgData.length);
		// TODO: TLS
		mState = State.Encrypted;
		mMuxChannels.put((short)0, new MuxChannelZero(this));
	}





	/** Handles the `dsms` cleartext protocol message.
	Checks the protocol version, and sends our own data. */
	private void handleDsmsMessage(byte[] aMsgData)
	{
		ByteArrayReader bar = new ByteArrayReader(aMsgData);
		if (!bar.checkAsciiString("Deskemes"))
		{
			close();
		}
		try
		{
			int protocolVersion = bar.readBE16();
			if (protocolVersion > 1)
			{
				Log.d(TAG, "Remote is using higher protocol version: " + protocolVersion);
			}

			mHasReceivedDsms = true;

			// Send our own `dsms` packet:
			mPacketBuilder.writeAsciiLiteral("Deskemes");
			mOutgoingPacket.putShort((short)1);
			sendCleartextMessage("dsms");

			// Send our info:
			// TODO: Proper values and wait with pubk / stls until proper time
			mOutgoingPacket.put(Utils.stringToUtf8("DummyFriendlyName"));
			sendCleartextMessage("fnam");
			mOutgoingPacket.put(Utils.stringToUtf8("DummyPublicID"));
			sendCleartextMessage("pubi");
		}
		catch (ByteArrayReader.DataEndReachedException exc)
		{
			close();
		}

	}





	/** Writes the packet data in mOutgoingPacket with the specified aMsgType to mOutgoingData
	as a next cleartext packet to send. */
	private void sendCleartextMessage(String aMsgType)
	{
		Log.d(TAG, "Sending cleartext message, type " + aMsgType);
		assert(mState != State.Encrypted);
		assert(aMsgType.length() == 4);

		// If the payload is too large, refuse:
		if (mOutgoingPacket.position() > 65535)
		{
			Log.d(TAG, "Packet payload too large");
			throw new BufferOverflowException();
		}

		mOutgoingDataPacketBuilder.writeAsciiLiteral(aMsgType);
		mOutgoingData.putShort((short)mOutgoingPacket.position());
		mOutgoingPacket.flip();
		mOutgoingData.put(mOutgoingPacket);
		mOutgoingPacket.clear();

		// Assume we've been called from selectedRead() for parsing the protocol, so try writing immediately:
		selectedWrite();
	}





	/** Extracts and handles a single Mux-protocol message from mIncomingData.
	Returns true if a message was extracted, false if not. */
	private boolean extractAndHandleMuxMessage()
	{
		int len = mIncomingData.limit();
		if (len < 4)
		{
			return false;
		}
		try
		{
			short channelID = mIncomingData.getShort();
			int msgLen = mIncomingData.getShort();
			if (msgLen < 0)
			{
				msgLen = 65536 + msgLen;
			}
			if (msgLen + 4 > len)
			{
				return false;
			}
			// A full message is present, process it:
			int pos = mIncomingData.position();
			byte[] msgData = Arrays.copyOfRange(mIncomingData.array(), pos, pos + msgLen);
			mIncomingData.position(pos + msgLen);
			handleMuxMessage(channelID, msgData);
		}
		catch (BufferUnderflowException exc)
		{
			return false;
		}
		return true;
	}





	/** Handles a single mux-protocol message. */
	private void handleMuxMessage(short aChannelID, byte[] aMsgData)
	{
		MuxChannel ch = mMuxChannels.get(aChannelID);
		if (ch == null)
		{
			Log.d(TAG, "Received a message for non-existent channel " + aChannelID);
			return;
		}
		ch.processMessage(aMsgData);
	}





	/** Sends the specified mux-protocol message to the remote. */
	private void sendMuxMessage(short mChannelID, byte[] aMessage)
	{
		mOutgoingData.putShort(mChannelID);
		mOutgoingData.putShort((short)(aMessage.length));
		mOutgoingData.put(aMessage);

		// Assume we've been called from selectedRead() for parsing the protocol, so try writing immediately:
		selectedWrite();
	}





	/** Returns the MuxChannel with the specified ID, or null if no such channel. */
	MuxChannel muxChannelByID(short aChannelID)
	{
		return mMuxChannels.get(aChannelID);
	}





	short registerChannel(MuxChannel aNewChannel) throws NoAvailableChannelIDException
	{
		for (int i = 1; i < 65536; ++i)
		{
			if (mMuxChannels.get((short)i) == null)
			{
				mMuxChannels.put((short)i, aNewChannel);
				return (short)i;
			}
		}
		throw new NoAvailableChannelIDException();
	}





	/** Closes the entire connection.
	Also called when the channel is closed from the remote end. */
	void close()
	{
		// Log the full stacktrace with the debug message:
		try
		{
			throw new Exception("Stacktrace dumper");
		}
		catch (Exception exc)
		{
			Log.d(TAG, "Closing connection", exc);
		}

		// Close the channel:
		try
		{
			mChannel.close();
		}
		catch (IOException exc)
		{
			Log.d(TAG, "Failed to close channel", exc);
		}

		// Notify ConnectionMgr:
		Log.d(TAG, "Connection to " + mRemoteAddress.toString() + " closed.");
		mConnMgr.connectionClosed(this);
	}





	InetAddress remoteAddress()
	{
		return mRemoteAddress.getAddress();
	}
}
