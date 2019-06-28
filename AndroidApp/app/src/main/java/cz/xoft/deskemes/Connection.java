package cz.xoft.deskemes;


import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.support.v4.app.NotificationCompat;
import android.support.v4.app.NotificationManagerCompat;
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





	// The individual cleartext message types (translated from ASCII strings to ints):
	private final static int msg_dsms = 0x64736d73;
	private final static int msg_fnam = 0x666e616d;
	private final static int msg_avtr = 0x61767472;
	private final static int msg_pair = 0x70616972;
	private final static int msg_pubi = 0x70756269;
	private final static int msg_pubk = 0x7075626b;
	private final static int msg_stls = 0x73746c73;




	/** The tag used for logging. */
	private final static String TAG = "Connection";

	/** The ID used for the notifications. */
	private final int mNotificationID = 1;

	/** The settings for the app. */
	private ServiceSettings mSettings;

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

	/** The remote address to which the connection will be made in the register() call. */
	private final InetSocketAddress mRemoteAddress;

	/** The selection key assigned by the selector in the register() call. */
	private SelectionKey mSelectionKey;

	/** Set to true once the `dsms` packet is received.
	Used to check that the `dsms` packet is the first one received; if not, the connection is aborted. */
	private boolean mHasReceivedDsms = false;

	/** Set to true once the `stls` message is received from the remote.
	Once true, all incoming data needs to be passed through TLS decoder.
	However, outgoing data may still be sent in cleartext (eg. pairing request) until we send the `stls` message. */
	private boolean mHasReceivedStls = false;

	/** Set to true once we send the `stls` message to the remote.
	Once true, all outgoing data needs to be passed through TLS encoder.
	However, incoming data may still be sent in cleartext (eg. pairing request) until we receive the `stls` message.
	See also mHasReceivedStls. */
	private boolean mHasSentStls = false;

	/** The name of the remote to be potentially shown to the user. */
	private String mFriendlyName;

	/** The PublicID presented by the remote in the `pubi` cleartext message. */
	private byte[] mRemotePublicID;

	/** The PublicID presented in the UDP beacon.
	Will be checked against the `pubi` cleartext message. */
	private final byte[] mBeaconPublicID;

	/** If mBeaconPublicID is already approved, contains the full approval information.
	Null if not approved yet. */
	private ApprovedPeers.Peer mApprovedPeer;

	/** The PublicKey presented by the remote in the `pubk` cleartext message. */
	private byte[] mRemotePublicKey;

	/** All the currently opened mux-protocol channels, indexed by their ChannelID. */
	private Map<Short, MuxChannel> mMuxChannels;

	/** The Context used for Android API calls. */
	private final Context mContext;

	/** The ConnectionMgr that takes care of this connection. */
	private ConnectionMgr mConnMgr;





	/** Creates a new instance that will connect to aRemoteAddress, and expect to find aBeaconPublicID there. */
	Connection(
		Context aContext,
		ServiceSettings aSettings,
		ConnectionMgr aConnMgr,
		InetSocketAddress aRemoteAddress,
		byte[] aBeaconPublicID,
		ApprovedPeers.Peer aPeer
	)
	{
		mContext = aContext;
		mSettings = aSettings;
		mConnMgr = aConnMgr;
		mRemoteAddress = aRemoteAddress;
		mBeaconPublicID = aBeaconPublicID;
		mApprovedPeer = aPeer;
		mIncomingData = ByteBuffer.allocate(MAX_INCOMING_DATA);
		mIncomingData.order(ByteOrder.BIG_ENDIAN);
		mOutgoingData = ByteBuffer.allocate(MAX_OUTGOING_DATA);
		mOutgoingData.order(ByteOrder.BIG_ENDIAN);
		mOutgoingDataPacketBuilder = new PacketBuilder(mOutgoingData);
		mMuxChannels = new HashMap<>();
	}





	byte[] remotePublicID()
	{
		return mRemotePublicID;
	}





	InetAddress remoteAddress()
	{
		return mRemoteAddress.getAddress();
	}





	Context context()
	{
		return mContext;
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
		if (mHasReceivedStls)
		{
			// TODO: TLS and extract+process
			return extractAndHandleMuxMessage();
		}
		else
		{
			return extractAndHandleCleartextMessage();
		}
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
			case msg_fnam: handleFnamMessage(aMsgData); break;
			case msg_pair: handlePairMessage(aMsgData); break;
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





	/** Handles the `fnam` cleartext protocol message.
	Stores the received friendly name. */
	private void handleFnamMessage(byte[] aMsgData)
	{
		mFriendlyName = Utils.utf8ToString(aMsgData);
	}





	/** Handles the `pair` cleartext protocol message.
	Notifies the user that there's a pairing request. */
	private void handlePairMessage(byte[] aMsgData)
	{
		mApprovedPeer = null;  // Clear any pairing we might have had for this connection
		notifyUserUnapprovedPeer();
	}





	/** Handles the `pubi` cleartext protocol message.
	Verifies that the public ID is the same as received in the beacon. */
	private void handlePubiMessage(byte[] aMsgData)
	{
		mRemotePublicID = aMsgData;
		if ((mBeaconPublicID != null) && !Arrays.equals(aMsgData, mBeaconPublicID))
		{
			close();
			return;
		}

		// If the approved peer has not yet been retrieved, try retrieving it now:
		if (mApprovedPeer == null)
		{
			mApprovedPeer = mConnMgr.approvedPeers().getPeer(mRemotePublicID);
		}
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

		if (mApprovedPeer == null)
		{
			// Not approved yet, wait until it asks for TLS or pairing, then show a pairing request UI
			return;
		}
		if (!Arrays.equals(mApprovedPeer.mRemotePublicKeyData, aMsgData))
		{
			// The public key doesn't match what we were paired against; most likely a MITM
			// Handle the same way as non-approved for now
			notifyUserUnapprovedPeer();
			mApprovedPeer = null;
			return;
		}

		// Send our own  pubkey:
		sendCleartextMessage("pubk", mApprovedPeer.mLocalPublicKeyData);
	}





	/** Handles the `stls` cleartext protocol message.
	Checks that we have everything needed for TLS, then switches to the TLS Mux protocol. */
	private void handleStlsMessage(byte[] aMsgData)
	{
		if ((mRemotePublicID == null) || (mRemotePublicKey == null))
		{
			Log.d(TAG, "Trying to start TLS without PubID / PubKey, closing connection.");
			close();
			return;
		}

		mHasReceivedStls = true;
		if (mApprovedPeer == null)
		{
			// Remote has an approval, but we don't. Inform the remote that we're pairing
			notifyUserUnapprovedPeer();
			sendCleartextMessage("pair");
			return;
		}

		Log.d(TAG, "Starting TLS, msgData length = " + aMsgData.length);

		if (!mHasSentStls)
		{
			sendCleartextMessage("stls");
		}

		// TODO: TLS
		mMuxChannels.put((short)0, new MuxChannelZero(this));
	}





	/** Displays a UI notification that an unapproved peer is attempting connection. */
	private void notifyUserUnapprovedPeer()
	{
		Log.d(TAG, "Remote " + mFriendlyName + " (" + mRemoteAddress + ") is trying to connect, but is not paired, creating pairing notification.");

		if (mFriendlyName == null)
		{
			// Don't want to show name-less requests
			return;
		}

		Intent intent = new Intent(mContext, ApprovePeerActivity.class);
		intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
		intent.putExtra("RemoteFriendlyName", mFriendlyName);
		intent.putExtra("RemoteAddress", mRemoteAddress.toString());
		intent.putExtra("RemotePublicID", mRemotePublicID);
		intent.putExtra("RemotePublicKey", mRemotePublicKey);
		PendingIntent pendingIntent = PendingIntent.getActivity(mContext, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);

		String txt = mContext.getResources().getString(R.string.pairing_notification_text);
		NotificationCompat.Builder nb = new NotificationCompat.Builder(mContext)
			.setSmallIcon(R.drawable.pairing_notification)
			.setContentIntent(pendingIntent)
			.setAutoCancel(true)
			.setContentText(txt)
			.setContentTitle(mFriendlyName);

		NotificationManagerCompat notificationManager = NotificationManagerCompat.from(mContext);
		notificationManager.notify(mNotificationID, nb.build());

		// We cannot allow an attacker to DoS this device by simply requesting pairings for many public IDs
		// We need to wait with the generating until the user actually initiates the approval.
		// But we can still send the `pair` message to let the other side know that we'll be pairing.
		sendCleartextMessage("pair");
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
		}
		catch (ByteArrayReader.DataEndReachedException exc)
		{
			close();
			return;
		}

		// Send our own `dsms` packet:
		byte[] packet = {0x44, 0x65, 0x73, 0x6b, 0x65, 0x6d, 0x65, 0x73, 0x00, 0x01};
		sendCleartextMessage("dsms", packet);

		// Send our info:
		sendCleartextMessage("fnam", Utils.stringToUtf8(mSettings.getFriendlyName()));
		sendCleartextMessage("pubi", mSettings.getPublicID());
	}





	/** Sends a cleartext packet with no payload. */
	private void sendCleartextMessage(String aMsgType)
	{
		sendCleartextMessage(aMsgType, new byte[]{});
	}





	/** Writes the packet data in aMsgPayload with the specified aMsgType to mOutgoingData
	as a next cleartext packet to send. */
	private void sendCleartextMessage(String aMsgType, byte[] aMsgPayload)
	{
		Log.d(TAG, "Sending cleartext message, type " + aMsgType + ", data length " + aMsgPayload.length);

		if (mHasSentStls)
		{
			Log.d(TAG, "Connection already encrypted.");
			return;
		}
		if (aMsgType.length() != 4)
		{
			Log.d(TAG, "Bad message identifier");
			return;
		}

		// If the payload is too large, refuse:
		if (aMsgPayload.length > 65535)
		{
			Log.d(TAG, "Packet payload too large");
			throw new BufferOverflowException();
		}

		mOutgoingDataPacketBuilder.writeAsciiLiteral(aMsgType);
		mOutgoingData.putShort((short)aMsgPayload.length);
		mOutgoingData.put(aMsgPayload);

		if (aMsgType == "stls")
		{
			mHasSentStls = true;
		}

		mSelectionKey.interestOps(SelectionKey.OP_READ | SelectionKey.OP_WRITE);
		mConnMgr.wakeUpSelector();
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

		mSelectionKey.interestOps(SelectionKey.OP_READ | SelectionKey.OP_WRITE);
		mConnMgr.wakeUpSelector();
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





	/** If appropriate, sends the pubkey to the remote.
	Called externally by the peer approval process after the pubkey is generated. */
	void onPublicKeyGenerated(byte[] aLocalPublicKey)
	{
		if (!mHasReceivedDsms || mHasSentStls)
		{
			Log.d(TAG, "Don't want public key now");
			return;
		}
		Log.d(TAG, "Sending local public key to " + mRemoteAddress.toString());
		sendCleartextMessage("pubk", aLocalPublicKey);
	}





	/** If waiting for approval, resumes the connection and starts TLS.
	Called externally by the peer approval process once the user accepts the peer. */
	void onPeerApproved(ApprovedPeers.Peer aPeer)
	{
		if (
			mHasSentStls ||
			(mRemotePublicKey == null)
		)
		{
			return;
		}
		mApprovedPeer = aPeer;

		Log.d(TAG, "Starting TLS");
		sendCleartextMessage("stls");
	}
}
