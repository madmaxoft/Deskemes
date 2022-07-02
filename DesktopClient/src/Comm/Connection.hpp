#pragma once

#include <memory>
#include <QObject>
#include <QIODevice>
#include <QMutex>
#include "../Optional.hpp"
#include "../ComponentCollection.hpp"
#include "../../lib/PolarSSL-cpp/CallbackSslContext.h"
#include "DetectedDevices.hpp"





// fwd:
class ChannelZero;





/** A class that represents a single connection to a device.
A device can utilize multiple connections to the computer. The connections can use different transports
(TCP, USB, Bluetooth), each transport provides a QIODevice interface for the actual IO.
The Connection object handles the base protocol (unauthenticated - TLS handshake - TLS muxed).
The Connection also provides the transport name and kind that is shown to the user (text + icon).
Each connection is uniquely identified by its EnumeratorID. */
class Connection:
	public QObject,
	public std::enable_shared_from_this<Connection>,
	public CallbackSslContext::DataCallbacks
{
	using Super = QObject;
	Q_OBJECT

	/** The couter used to identify connections in the logs. */
	static std::atomic_int mCounter;


public:

	/** Base class for all comm channels muxed by the Connection (in csEncrypted state).
	Each descendant implements a specific channel protocol (such as filesystem access, information, phonebook, messages etc.)
	A channel uses the parent Connection's sendChannelMessage() function to send data to the app.
	The Connection calls the processIncomingMessage() function when a new message arrives for the channel over the connection. */
	class Channel;

	using ChannelPtr = std::shared_ptr<Channel>;


	enum TransportKind
	{
		tkUsb,        ///< Using USB + ADB as the underlying transport
		tkTcp,        ///< Using TCP/IP as the underlying transport
		tkBluetooth,  ///< Using Bluetooth as the underlying transport
	};


	enum State
	{
		csInitial,           ///< The connection is in the initial cleartext phase / no public key received
		csUnknownPairing,    ///< This side has found no pairing for this device, waiting for upper level to initiate pairing
		csKnownPairing,      ///< This side has determined that the remote is known, no pairing needed
		csRequestedPairing,  ///< The connection is waiting for pairing in the UI
		csBlacklisted,       ///< The device's public ID is in the blacklist
		csDifferentKey,      ///< The device's public key is different from the last time we paired (MITM?)
		csEncrypted,         ///< The connection is in the encrypted phase
		csDisconnected,      ///< The connection has been disconnected (but is kept for housekeeping)
	};


	/** Creates a new connection in the csInitial phase.
	The ConnectionID is used as a unique identifier of the connection, it is assigned by whoever enumerated
	the connection.
	The IO is used for the actual reading and writing data through the connection.
	The TransportKind and TransportName are primarily used for UI display to the user. */
	explicit Connection(
		ComponentCollection & aComponents,
		const QByteArray & aConnectionID,
		QIODevice * aIO,
		TransportKind aTransportKind,
		const QString & aTransportName,
		QObject * aParent = nullptr
	);


	// Simple getters:
	const QByteArray & connectionID() const { return mConnectionID; }
	TransportKind transportKind() const { return mTransportKind; }
	const Optional<QString> & friendlyName() const { return mFriendlyName; }
	const Optional<QByteArray> & avatar() const { return mAvatar; }
	const Optional<QByteArray> & remotePublicID() const { return mRemotePublicID; }
	const Optional<QByteArray> & remotePublicKeyData() const { return mRemotePublicKeyData; }
	State state() const { return mState; }

	// Simple setters:
	void setFriendlyName(const QString & aFriendlyName) { mFriendlyName = aFriendlyName; emit receivedFriendlyName(this); }
	void setAvatar(const QByteArray & aAvatar) { mAvatar = aAvatar; emit receivedAvatar(this); }

	/** Returns the (implied) enumerator kind that created this connection. */
	ComponentCollection::ComponentKind enumeratorKind() const { return enumeratorKindFromTransportKind(mTransportKind); }

	/** Translates the TransportKind into the respective enumerator kind. */
	static ComponentCollection::ComponentKind enumeratorKindFromTransportKind(TransportKind aTransportKind);

	/** Translates the State into DetectdDevices::Device::Status. */
	static DetectedDevices::Device::Status stateToDetectedDevicesStatus(State aState);

	/** Translates the State into a string representation, used mainly for logging. */
	static QString stateToString(State aState);

	/** Terminates the connection forcefully. */
	Q_INVOKABLE void terminate();

	/** Marks the connection as requesting pairing, and emits the requestingPairing signal. */
	Q_INVOKABLE void requestPairing();

	/** Stores the remote public ID.
	Checks whether the public ID + key combo is known or not, sets state accordingly. */
	void setRemotePublicID(const QByteArray & aPublicID);

	/** Stores the remote public key.
	Checks whether the public ID + key combo is known or not, sets state accordingly. */
	void setRemotePublicKey(const QByteArray & aPubKeyData);

	/** Sends the local public key to the remote.
	The public key is queried from DevicePairings (asserts that it's valid).
	Asserts that the protocol is still in the cleartext phase and StartTls hasn't been sent. */
	Q_INVOKABLE void sendLocalPublicKey();

	/** Sends the `pair` request to the remote.
	The remote should notify the user and ask them for pairing approval. */
	Q_INVOKABLE void sendPairingRequest();

	/** Called by the UI when the pairing has been approved locally.
	Stores the pairing in DevicePairings and sends the StartTLS request to the device. */
	Q_INVOKABLE void localPairingApproved();

	/** Opens a new channel on the muxed protocol with the specified Channel subclass.
	The aChannel parameter is the Channel subclass instance that will be used to handle the channel's data.
	Returns false if not in csEncrypted or in case of immediate errors.
	Returns true on success, but the channel itself is not yet confirmed from the device at that point.
	Users need to wait for the channel's opened() signal before they can send any data over the channel. */
	bool openChannel(
		ChannelPtr aChannel,
		const QByteArray & aServiceName,
		const QByteArray & aServiceInitData = QByteArray()
	);

	/** Returns the logger used for this connection. */
	Logger & logger() { return mLogger; }


protected:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The unique identifier of the connection, assigned by the enumerator. */
	const QByteArray mConnectionID;

	/** The actual IO used for communicating; provided by the transport that created the connection. */
	QIODevice * mIO;

	/** The kind of transport used. */
	TransportKind mTransportKind;

	/** The user-visible name of the transport used, such as "USB" or "WiFi". */
	QString mTransportName;

	/** The current state of the connection. */
	State mState;

	/** The friendly name, returned from the device. */
	Optional<QString> mFriendlyName;

	/** The avatar image data presented by the device. */
	Optional<QByteArray> mAvatar;

	/** The public ID identifying the remote device. */
	Optional<QByteArray> mRemotePublicID;

	/** The public key from the remote device, raw data. */
	Optional<QByteArray> mRemotePublicKeyData;

	/** Buffer for the unprocessed incoming data.
	The data is stored here until a full message can be extracted; it is then removed from the buffer.
	If the connection is in the TLS state, the data here is already decrypted. */
	QByteArray mIncomingData;

	/** Buffer for the unprocessed encrypted incoming data.
	Only used while in the TLS state, the data gets stored here before being sent through the TLS decryptor
	(because the decryptor is pull-based, while we have push-based incoming data). */
	QByteArray mIncomingDataEncrypted;

	/** Buffer for the unprocessed plain outgoing data.
	Only used while in the TLS state, the data gets stored here before being encrypted by TLS
	and sent to the remote. */
	QByteArray mOutgoingDataPlain;

	/** True after the initial protocol identification has been received.
	Used to detect messages sent without proper identification first. */
	bool mHasReceivedIdentification;

	/** The protocol version the remote has indicated in their protocol ident. */
	quint16 mRemoteVersion;

	/** Set to true after the StartTls request has been sent
	No more cleartext messages are allowed to be sent from us. */
	bool mHasSentStartTls;

	/** Channels that have been opened on the csEncrypted connection.
	Protected against multithreaded access by mMtxChannels. */
	std::map<quint16, ChannelPtr> mChannels;

	/** The mutex protecting mChannels against multithreaded access. */
	QMutex mMtxChannels;

	/** The logger used for all messages produced by this class. */
	Logger & mLogger;

	/** The interface to the TLS codec in PolarSSL,for decoding the TLS communication.
	Protexted against multithreaded access using mMtxTls. */
	std::unique_ptr<CallbackSslContext> mTls;

	/** The mutex protecting mTls against multithreaded access. */
	QMutex mMtxTls;


	/** Checks whether the remote public key and ID pair is known.
	If either is missing, silently bails out.
	If the pair is not known, sets state to csNeedPairing and emits the unknownPairing() signal.
	If the pair is known, sets state to csNoNeedPairing and emits the knownPairing() signal. */
	void checkRemotePublicKeyAndID();

	/** Sets the new state.
	Emits the stateChanged signal. */
	void setState(State aNewState);

	/** Processes the incoming data.
	Based on the current protocol state, handles the data as cleartext or TLSMux. */
	void processIncomingData(const QByteArray & aData);

	/** Extracts a complete cleartext protocol message from mIncomingData, and acts on it.
	Returns true if a complete message was extracted, false if there's no complete message. */
	bool extractAndHandleCleartextMessage();

	/** Handles the specified message. */
	void handleCleartextMessage(const quint32 aMsgType, const QByteArray & aMsg);

	/** Handles the "dsms" message. */
	void handleCleartextMessageDsms(const QByteArray & aMsg);

	/** Handles the "stlt" message, starting the TLS, if appropriate.
	If the protocol has been violated (missing id / key), the connection terminates. */
	void handleCleartextMessageStls();

	/** Sends the specified message over to the remote peer. */
	void sendCleartextMessage(quint32 aMsgType, const QByteArray & aMsg = QByteArray());

	/** Extracts a complete mux protocol (tls) message from mIncomingData, and distributes it to the proper channel.
	Returns true if a complete message was extracted, false if there's no complete message. */
	bool extractAndHandleMuxMessage();

	/** Returns the channel identified by its ID, or nullptr if no such channel. */
	ChannelPtr channelByID(quint16 aChannelID);

	/** Returns the ChannelZero instance associated with this connection, or nullptr if the conenction
	doesn't have the command channel assigned yet. */
	std::shared_ptr<ChannelZero> channelZero();

	/** Adds the specified channel to our internal channel map. */
	void addChannel(const quint16 aChannelID, ChannelPtr aChannel);

	/** Sends the specified message through the connection to the remote channel specified.
	Asserts that the connection is csEncrypted.
	To be used from Channel::sendMessage() only. */
	void sendChannelMessage(const quint16 aChannelID, const QByteArray & aMessage);

	/** Processes the incoming and outgoing TLS data.
	Feeds data from mIncomingDataEncrypted to TLS, sends the encrypted data produced by TLS.
	Keeps processing until both the incoming and outgoing buffers are empty. */
	void processTls();

	// CallbackSslContext::DataCallbacks overrides:
	virtual int receiveEncrypted(unsigned char * aBuffer, size_t aNumBytes) override;
	virtual int sendEncrypted(const unsigned char * aBuffer, size_t aNumBytes) override;


signals:

	/** Emitted when the connection to the device is lost. */
	void disconnected(Connection * aConnection);

	/** Emitted when the connection is fully established (has TLS, has ChannelZero). */
	void established(Connection * aConnection);

	/** Emitted when the connection detects that pairing with the remote peer is necessary
	(either a "pair" request comes from the peer, or we sent it to them.
	The receiver should also hook the disconnected() signal and dismiss the pairing UI if it fires. */
	void requestingPairing(Connection * aConnection);

	/** Emitted when the remote sends an unknown PublicID.
	The receiver should either initiate pairing via initiatePairing(), or drop the entire connection. */
	void unknownPairing(Connection * aConnection);

	/** Emitted when the remote sends a known id+key pair.
	The remote may still need to do pairing; if so, the requestingPairing() will be signalled later. */
	void knownPairing(Connection * aConnection);

	/** Emitted when the remote sends an id+key pair and we have a different key on file.
	This is most likely a MITM attack, so the connection should be dropped soon (unless detecting devices to overwrite old key). */
	void differentKey(Connection * aConnection);

	/** Emitted when the remote public ID is received. */
	void receivedPublicID(Connection * aConnection);

	/** Emitted when the remote public key is received. */
	void receivedPublicKey(Connection * aConnection);

	/** Emitted when the friendly name is received. */
	void receivedFriendlyName(Connection * aConnection);

	/** Emitted when the avatar is received. */
	void receivedAvatar(Connection * aConnection);

	/** Emitted after the state has changed. */
	void stateChanged(Connection * aConnection, State aNewState);


public slots:


private slots:

	/** The mIO has data available for reading.
	Reads the data and pushes it into the current phase. */
	void ioReadyRead();

	/** The mIO is closing. */
	void ioClosing();
};

using ConnectionPtr = std::shared_ptr<Connection>;

Q_DECLARE_METATYPE(Connection *);
Q_DECLARE_METATYPE(ConnectionPtr);





class Connection::Channel:
	public QObject,
	public std::enable_shared_from_this<Connection::Channel>
{
	using super = QObject;

	Q_OBJECT


public:

	// Allow the control channel full access to this object (for opening and closing purposes):
	friend class ChannelZero;


	/** Basic error codes */
	enum
	{
		ERR_UNSUPPORTED_REQTYPE = 1,
		ERR_NO_SUCH_SERVICE = 2,
		ERR_NO_CHANNEL_ID = 3,
		ERR_SERVICE_INIT_FAILED = 4,
		ERR_NO_PERMISSION = 5,
		ERR_NOT_YET = 6,
		ERR_NO_REQUEST_ID = 7,   ///> Internal comm error: There is no free RequestID to send this request (Ch0)
		ERR_DISCONNECTED = 8,    ///> Internal comm error: The connection has been disconnected
	};


	/** Creates a new instance, bound to the specified connection. */
	explicit Channel(Connection & aConnection);

	// Simple getters:
	Connection & connection() const { return mConnection; }
	quint16 channelID() const { return mChannelID; }
	bool isOpen() const { return mIsOpen; }

	/** Called by the parent connection when a new message arrives for the channel from the device.
	Descendants use this to implement the receiving side of the protocol. */
	virtual void processIncomingMessage(const QByteArray & aMessage) = 0;

	/** Sends the specified message through the connection to the device.
	Asserts that the connection has been actually opened (mIsOpen). */
	void sendMessage(const QByteArray & aMessage);


protected:

	/** The connection in which the channel lives. */
	Connection & mConnection;

	/** The channel ID used in the connection for this channel. */
	quint16 mChannelID;

	/** Set to true once the device confirms opening the channel.
	Sending data on a non-open channel is a program logic error and will be caught in sendMessage(). */
	bool mIsOpen;


signals:

	/** The channel has been confirmed open by the device. */
	void opened(Channel * aSelf);

	/** The channel has just been closed by the device. */
	void closed(Channel * aSelf);

	/** The channel has failed to open on the device. */
	void failed(Channel * aSelf, const quint16 aErrorCode, const QByteArray & aErrorMessage);
};
