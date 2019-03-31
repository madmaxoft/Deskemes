#pragma once

#include <memory>
#include <QObject>
#include <QIODevice>
#include "../Optional.hpp"
#include "../ComponentCollection.hpp"





// fwd:
class PhaseHandler;





/** A class that represents a single connection to a device.
A device can utilize multiple connections to the computer. The connections can use different transports
(TCP, USB, Bluetooth), each transport provides a QIODevice interface for the actual IO.
The Connection object handles the common protocol phasing (unauthenticated - TLS handshake - TLS muxed).
The Connection also provides the transport name and kind that is shown to the user (text + icon).
Upon receiving data from the remote, it calls the current PhaseHandler's processIncomingData() method.
To send data to the remote, call write(). */
class Connection:
	public QObject
{
	using Super = QObject;
	Q_OBJECT


public:

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
	};


	/** Creates a new connection in the Unauthenticated phase. */
	explicit Connection(
		ComponentCollection & aComponents,
		QIODevice * aIO,
		TransportKind aTransportKind,
		const QString & aTransportName,
		QObject * aParent = nullptr
	);


	// Simple getters:
	const Optional<QString> & friendlyName() const { return mFriendlyName; }
	const Optional<QByteArray> & avatar() const { return mAvatar; }
	const Optional<QByteArray> & remotePublicID() const { return mRemotePublicID; }
	const Optional<QByteArray> & remotePublicKeyData() const { return mRemotePublicKeyData; }
	State state() const { return mState; }

	// Simple setters:
	void setFriendlyName(const QString & aFriendlyName) { mFriendlyName = aFriendlyName; emit receivedFriendlyName(this); }
	void setAvatar(const QByteArray & aAvatar) { mAvatar = aAvatar; emit receivedAvatar(this); }

	/** Terminates the connection forcefully. */
	void terminate();

	/** Writes the specified data to the remote end. */
	void write(const QByteArray & aData);

	/** Marks the connection as requesting pairing, and emits the requestingPairing signal. */
	void requestPairing();

	/** Changes the PhaseHandler to the specified one, and returns the old one.
	Also updates the internal state to the specified state. */
	std::unique_ptr<PhaseHandler> setPhaseHandler(
		std::unique_ptr<PhaseHandler> && aNewHandler,
		State aNewState
	);

	/** Stores the remote public ID.
	Checks whether the public ID + key combo is known or not, sets state accordingly. */
	void setRemotePublicID(const QByteArray & aPublicID);

	/** Stores the remote public key.
	Checks whether the public ID + key combo is known or not, sets state accordingly. */
	void setRemotePublicKey(const QByteArray & aPubKeyData);


protected:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The actual IO used for communicating; provided by the transport that created the connection. */
	QIODevice * mIO;

	/** The kind of transport used. */
	TransportKind mTransportKind;

	/** The user-visible name of the transport used, such as "USB" or "WiFi". */
	QString mTransportName;

	/** The handler that processes the incoming data. */
	std::unique_ptr<PhaseHandler> mPhaseHandler;

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


	/** Checks whether the remote public key and ID pair is known.
	If either is missing, silently bails out.
	If the pair is not known, sets state to csNeedPairing and emits the unknownPairing() signal.
	If the pair is known, sets state to csNoNeedPairing and emits the knownPairing() signal. */
	void checkRemotePublicKeyAndID();

	/** Sets the new state.
	Emits the stateChanged signal. */
	void setState(State aNewState);


signals:

	/** Emitted when the connection to the device is lost. */
	void disconnected(Connection * aConnection);

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
	void stateChanged(State aNewState);


public slots:


private slots:

	/** The mIO has data available for reading.
	Reads the data and pushes it into the current phase. */
	void ioReadyRead();

	/** The mIO is closing. */
	void ioClosing();
};
