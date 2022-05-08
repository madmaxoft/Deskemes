#include "Connection.hpp"
#include <cassert>
#include <QTimer>
#include <QDateTime>
#include "../../lib/PolarSSL-cpp/SslConfig.h"
#include "../../lib/PolarSSL-cpp/X509Cert.h"
#include "../../lib/PolarSSL-cpp/CryptoKey.h"
#include "../../lib/PolarSSL-cpp/TlsException.h"
#include "../InstallConfiguration.hpp"
#include "../Utils.hpp"
#include "../DB/DevicePairings.hpp"





/** If defined, TLS messages below the threshold will be logged through the connection's Logger. */
// #define DEBUG_TLS 5





/** Implemented in MbedTls, sets the threshold for debug messages reported via the debug hook.
threshold of 0 turns off all messages (default)
threshold of 5 turns all messages on. */
extern "C" void mbedtls_debug_set_threshold(int threshold);





/** Callback that can be used in mbedTls for receiving the TLS debug messages. */
static void tlsDebugCallback(void * aCallbackData, int aDebugLevel, const char * aFileName, int aLineNumber, const char * aMessage)
{
	Q_UNUSED(aDebugLevel);
	Q_UNUSED(aFileName);
	Q_UNUSED(aLineNumber);

	auto conn = reinterpret_cast<Connection *>(aCallbackData);
	conn->logger().log("TLS: %1", aMessage);
}





/** Returns true if the two keys are the same.
The weird signature is due to data types used at the only call site:
Key is stored in the DB in a QByteArray, but is received from TLS in a std::string.
This function does a byte-by-byte comparison of the binary data. */
static bool isSameKey(const QByteArray & aKey1, const std::string & aKey2)
{
	if (aKey1.length() != static_cast<int>(aKey2.size()))
	{
		return false;
	}
	return (memcmp(aKey1.constData(), aKey2.data(), aKey2.size()) == 0);
}





/** Callback for certificate verification. */
static int tlsVerifyCallback(void * aUserData, mbedtls_x509_crt * aCurrentCert, int aChainDepth, uint32_t * aVerificationFlags)
{
	Q_UNUSED(aChainDepth);

	auto conn = reinterpret_cast<Connection *>(aUserData);
	conn->logger().log("Verifying certificate...");
	auto cert = X509Cert::fromContext(aCurrentCert);
	std::string pubKeyDer;
	try
	{
		pubKeyDer = cert->publicKeyDer();
	}
	catch (const TlsException & exc)
	{
		conn->logger().log("Failed to extract device's public key: %1.", exc.what());
		*aVerificationFlags = 0xffffffff;
		return -1;
	}
	if (isSameKey(conn->remotePublicKeyData().value().constData(), pubKeyDer))
	{
		conn->logger().log("The key matches");
		*aVerificationFlags = 0;
		return 0;
	}
	conn->logger().log("ERROR: Key MISMATCH");
	*aVerificationFlags = 0xffffffff;
	return -1;
}





////////////////////////////////////////////////////////////////////////////////
// ChannelZero:

/** The implementation of the special channel zero - the one that manages all the other channels. */
class ChannelZero:
	public Connection::Channel
{
	using Super = Connection::Channel;

	Q_OBJECT


public:

	ChannelZero(Connection & aConnection):
		Super(aConnection)
	{
		// Set the fixed channel ID, no opening necessary for this special channel:
		mChannelID = 0;
		mIsOpen = true;

		// Add a ping-timer
		connect(&mTimer, &QTimer::timeout, this, &ChannelZero::sendPing);
		mTimer.start(1000);  // 1 second
	}




	virtual ~ChannelZero() override
	{
		// Stop the ping-timer:
		mTimer.stop();

		// Call the failure handler of each pending request:
		QMutexLocker lock(&mMtx);
		for (auto & req: mPendingRequests)
		{
			(*req.second).mErrorHandler(ERR_DISCONNECTED, "The connection was lost");
		}
	}





	/** Sends the request to open a new connection for the specified Channel descendant instance.
	Note that aChannel is still not opened at this point, clients need to wait for the opened() or failed()
	signals on the Channel before writing data to it. */
	void openChannel(
		Connection::ChannelPtr aChannel,
		const QByteArray & aServiceName,
		const QByteArray & aServiceInitData
	)
	{
		assert(!aChannel->mIsOpen);
		assert(aChannel->channelID() == 0);

		QByteArray req;
		Utils::writeBE16Lstring(req, aServiceName);
		Utils::writeBE16Lstring(req, aServiceInitData);
		sendRequest("open"_4cc,
			// Success handler:
			[this, aChannel, serviceName = aServiceName](const QByteArray & aAdditionalData)
			{
				aChannel->mChannelID = Utils::readBE16(aAdditionalData);
				aChannel->mIsOpen = true;
				mConnection.logger().log("Channel \"%1\" acknowledged by the device.", serviceName);
				emit channelAcknowledged(aChannel);
				emit aChannel->opened(aChannel.get());
			},

			// Failure handler:
			[aChannel](const quint16 aErrorCode, const QByteArray & aErrorMessage)
			{
				emit aChannel->failed(aChannel.get(), aErrorCode, aErrorMessage);
			},
			req
		);
	}





signals:

	/** Emitted after the device acknowledges opening the specified new channel. */
	void channelAcknowledged(Connection::ChannelPtr aChannel);


protected:

	/** Storage for a request while it is being serviced by the remote.
	Stays in mPendingRequests until a Response or an Error for it arrive. */
	struct PendingRequest
	{
		using SuccessHandler = std::function<void(const QByteArray & aAdditionalData)>;
		using ErrorHandler = std::function<void(const quint16 aErrorCode, const QByteArray & aErrorMsg)>;

		quint8 mRequestID;
		SuccessHandler mSuccessHandler;
		ErrorHandler mErrorHandler;

		PendingRequest(
			quint8 aRequestID,
			SuccessHandler aSuccessHandler,
			ErrorHandler aErrorHandler
		):
			mRequestID(aRequestID),
			mSuccessHandler(aSuccessHandler),
			mErrorHandler(aErrorHandler)
		{
		}
	};


	/** The requests that have been sent to the remote and haven't been answered yet.
	Protected against multithreaded access by mMtx. */
	std::map<quint8, std::unique_ptr<PendingRequest>> mPendingRequests;

	/** The mutex protecting mPendingRequests against multithreaded access. */
	QMutex mMtx;




	// Channel override:
	virtual void processIncomingMessage(const QByteArray & aMessage) override
	{
		if (aMessage.size() < 2)
		{
			qWarning() << "Message too small: " << aMessage.size();
			return;
		}
		switch (aMessage[0])
		{
			case 0x00:
			{
				handleRequest(aMessage);
				break;
			}
			case 0x01:
			{
				handleResponse(aMessage);
				break;
			}
			case 0x02:
			{
				handleError(aMessage);
				break;
			}
			default:
			{
				qWarning() << "Unhandled message request type: " << static_cast<int>(aMessage[0]);
				break;
			}
		}
	}





	/** Handles a Request message from the remote.
	The aMessage contains the entire serialized message (incl. MsgType, ReqID and ReqType).*/
	void handleRequest(const QByteArray & aMessage)
	{
		if (aMessage.size() < 6)
		{
			qWarning() << "Request too small: " << aMessage.size();
			return;
		}
		auto reqID = static_cast<quint8>(aMessage[1]);
		auto reqType = Utils::readBE32(aMessage, 2);
		switch (reqType)
		{
			case "ping"_4cc:
			{
				sendResponse(reqID, aMessage.mid(6));
				break;
			}
			// "open" and "clse" requests are not supported, handled in "default"
			default:
			{
				qWarning() << "Unsupported request received: " << Utils::writeBE32(reqType);
				sendError(reqID, ERR_UNSUPPORTED_REQTYPE);
				break;
			}
		}
	}





	/** Handles a Response message from the remote.
	The aMessage contains the entire serialized message (incl. MsgType and ReqID). */
	void handleResponse(const QByteArray & aMessage)
	{
		if (aMessage.size() < 2)
		{
			qWarning() << "Response too small: " << aMessage.size();
			return;
		}
		auto requestID = static_cast<quint8>(aMessage[1]);
		QMutexLocker lock(&mMtx);
		auto itr = mPendingRequests.find(requestID);
		if (itr == mPendingRequests.end())
		{
			qWarning() << "Received a response for a non-existent request ID " << requestID;
			return;
		}
		auto req = std::move(itr->second);
		mPendingRequests.erase(itr);
		lock.unlock();
		req->mSuccessHandler(aMessage.mid(2));
	}





	/** Handles an Error message from the remote.
	The aMessage contains the entire serialized message (incl. MsgType and ReqID). */
	void handleError(const QByteArray & aMessage)
	{
		if (aMessage.size() < 4)
		{
			qWarning() << "Response too small: " << aMessage.size();
			return;
		}
		auto requestID = static_cast<quint8>(aMessage[1]);
		auto errorCode = Utils::readBE16(aMessage, 2);
		QMutexLocker lock(&mMtx);
		auto itr = mPendingRequests.find(requestID);
		if (itr == mPendingRequests.end())
		{
			qWarning() << "Received an error response for a non-existent request ID " << requestID;
			return;
		}
		auto req = std::move(itr->second);
		mPendingRequests.erase(itr);
		lock.unlock();
		req->mErrorHandler(errorCode, aMessage.mid(4));
	}





	/** Sends a success response to a previous request to the remote. */
	void sendResponse(const quint8 aRequestID, const QByteArray & aMsgData = QByteArray())
	{
		QByteArray buf;
		buf.push_back(0x01);
		buf.push_back(static_cast<char>(aRequestID));
		buf.append(aMsgData);
		sendMessage(buf);
	}





	/** Sends an error response to a previous request to the remote. */
	void sendError(
		const quint8 aRequestID,
		const quint16 aErrorCode,
		const QByteArray & aMsgData = QByteArray()
	)
	{
		QByteArray buf;
		buf.push_back(0x02);
		buf.push_back(static_cast<char>(aRequestID));
		Utils::writeBE16(buf, aErrorCode);
		buf.append(aMsgData);
		sendMessage(buf);
	}





	/** Sends a request and calls the specified handler when response or error is received.
	Automatically assigns a request ID.
	The error handler is called immediately if there's no free request IDs, with ERR_NO_REQUEST_ID. */
	void sendRequest(
		const quint32 aRequestType,
		PendingRequest::SuccessHandler aSuccessHandler,
		PendingRequest::ErrorHandler aErrorHandler,
		const QByteArray & aAdditionalData = QByteArray()
	)
	{
		QMutexLocker lock(&mMtx);
		if (mPendingRequests.size() >= 255)
		{
			lock.unlock();
			aErrorHandler(ERR_NO_REQUEST_ID, "Too many pending requests, no free ID to assign to this one.");
			return;
		}

		// Find a free request ID to assign:
		quint8 reqID = 255;
		for (quint8 i = 0; i < 255; i++)
		{
			if (mPendingRequests.find(i) == mPendingRequests.end())
			{
				reqID = i;
				break;
			}
		}

		// Store the pending request:
		mPendingRequests[reqID] = std::make_unique<PendingRequest>(
			reqID,
			aSuccessHandler,
			aErrorHandler
		);
		lock.unlock();

		// Serialize and send the request:
		QByteArray req;
		req.push_back('\0');  // Request
		req.push_back(static_cast<char>(reqID));
		Utils::writeBE32(req, aRequestType);
		req.append(aAdditionalData);
		sendMessage(req);
	}





	/** Called when a ping's response is received, with the calculated roundtrip time. */
	void pingReceived(qint64 aRoundtripMsec)
	{
		Q_UNUSED(aRoundtripMsec);

		// TODO: Store and process the value
		// mLogger.log("Ping received in %1 msec.", aRoundtripMsec);
	}


private slots:

	void sendPing()
	{
		static int numPings = 0;
		auto now = QDateTime::currentMSecsSinceEpoch();
		sendRequest(
			"ping"_4cc,
			[this, now](const QByteArray & aAdditionalData)
			{
				Q_UNUSED(aAdditionalData);
				this->pingReceived(QDateTime::currentMSecsSinceEpoch() - now);
			},
			[=](const quint16 aErrorCode, const QByteArray & aErrorMessage)
			{
				mConnection.logger().log("Ping request returned an error: %1; %2", aErrorCode, aErrorMessage);
			},
			QString::number(numPings++).toUtf8()
		);
	}


protected:

	/** The timer used for pinging. */
	QTimer mTimer;
};

#include "Connection.moc"





////////////////////////////////////////////////////////////////////////////////
// Connection::Channel:

Connection::Channel::Channel(Connection & aConnection):
	mConnection(aConnection),
	mChannelID(0),
	mIsOpen(false)
{
}





void Connection::Channel::sendMessage(const QByteArray & aMessage)
{
	assert(mIsOpen);
	mConnection.sendChannelMessage(mChannelID, aMessage);
}





////////////////////////////////////////////////////////////////////////////////
// Connection:

Connection::Connection(
	ComponentCollection & aComponents,
	const QByteArray & aConnectionID,
	QIODevice * aIO,
	TransportKind aTransportKind,
	const QString & aTransportName,
	QObject * aParent
):
	Super(aParent),
	mComponents(aComponents),
	mConnectionID(aConnectionID),
	mIO(aIO),
	mTransportKind(aTransportKind),
	mTransportName(aTransportName),
	mState(csInitial),
	mHasReceivedIdentification(false),
	mRemoteVersion(0),
	mHasSentStartTls(false),
	mLogger(aComponents.logger("Connection-" + aConnectionID))
{
	connect(aIO, &QIODevice::readyRead,           this, &Connection::ioReadyRead);
	connect(aIO, &QIODevice::aboutToClose,        this, &Connection::ioClosing);
	connect(aIO, &QIODevice::readChannelFinished, this, &Connection::ioClosing);

	// Send the protocol identification:
	mLogger.log("Sending protocol identification...");
	QByteArray protocolIdent("Deskemes");
	Utils::writeBE16(protocolIdent, 1);  // version
	sendCleartextMessage("dsms"_4cc, protocolIdent);

	// If there's data already on the connection, process it:
	if (aIO->waitForReadyRead(1))
	{
		ioReadyRead();
	}
}





ComponentCollection::ComponentKind Connection::enumeratorKindFromTransportKind(Connection::TransportKind aTransportKind)
{
	switch (aTransportKind)
	{
		case tkUsb: return ComponentCollection::ckUsbDeviceEnumerator;
		case tkTcp: return ComponentCollection::ckTcpListener;
		case tkBluetooth: return ComponentCollection::ckBluetoothDeviceEnumerator;
	}
	#ifdef _MSC_VER
		assert(!"Invalid transport kind");
		throw LogicError("Invalid transport kind: %1", aTransportKind);
	#endif
}





void Connection::terminate()
{
	mIO->close();
}





void Connection::requestPairing()
{
	assert(mState != csEncrypted);
	setState(csRequestedPairing);
	emit requestingPairing(this);
}





void Connection::setRemotePublicID(const QByteArray & aPublicID)
{
	mRemotePublicID = aPublicID;
	emit receivedPublicID(this);

	// If we have a pairing to the device, send our pubkey now:
	auto pairings = mComponents.get<DevicePairings>();
	auto pairing = pairings->lookupDevice(mRemotePublicID.value());
	if (pairing.isPresent())
	{
		sendCleartextMessage("pubk"_4cc, pairing.value().mLocalPublicKeyData);
	}
	else
	{
		setState(csUnknownPairing);
		emit unknownPairing(this);
	}

	checkRemotePublicKeyAndID();
}





void Connection::setRemotePublicKey(const QByteArray & aPubKeyData)
{
	mRemotePublicKeyData = aPubKeyData;
	emit receivedPublicKey(this);
	checkRemotePublicKeyAndID();
}





void Connection::sendLocalPublicKey()
{
	assert(mState != csEncrypted);
	assert(!mHasSentStartTls);

	auto pairing = mComponents.get<DevicePairings>()->lookupDevice(mRemotePublicID.value());
	assert(pairing.isPresent());

	mLogger.log("Sending public key data");
	sendCleartextMessage("pubk"_4cc, pairing.value().mLocalPublicKeyData);
}





void Connection::sendPairingRequest()
{
	mLogger.log("Sending pairing request");
	sendCleartextMessage("pair"_4cc);
}





void Connection::localPairingApproved()
{
	assert(
		(mState == csUnknownPairing) ||
		(mState == csKnownPairing) ||
		(mState == csRequestedPairing) ||
		(mState == csDifferentKey) ||
		(mState == csEncrypted)  // The device has already started encryption
	);
	assert(mRemotePublicID.isPresent());
	assert(mRemotePublicKeyData.isPresent());
	const auto & id = mRemotePublicID.value();
	auto pairings = mComponents.get<DevicePairings>();
	auto pairing = pairings->lookupDevice(id);
	assert(pairing.isPresent());
	assert(!pairing.value().mLocalPublicKeyData.isEmpty());
	assert(!pairing.value().mLocalPrivateKeyData.isEmpty());

	pairings->pairDevice(
		mFriendlyName.valueOr(Utils::toHex(id)),
		id,
		mRemotePublicKeyData.value(),
		pairing.value().mLocalPublicKeyData,
		pairing.value().mLocalPrivateKeyData
	);

	if (mState != csEncrypted)
	{
		sendCleartextMessage("stls"_4cc);
	}
}





bool Connection::openChannel(
	ChannelPtr aChannel,
	const QByteArray & aServiceName,
	const QByteArray & aServiceInitData
)
{
	if (mState != csEncrypted)
	{
		mLogger.log("Invalid protocol state, %1 instead of %2 (encrypted).", mState, csEncrypted);
		return false;
	}
	auto ch0 = channelZero();
	assert(ch0 != nullptr);
	ch0->openChannel(aChannel, aServiceName, aServiceInitData);
	return true;
}





void Connection::checkRemotePublicKeyAndID()
{
	if (!mRemotePublicID.isPresent() || !mRemotePublicKeyData.isPresent())
	{
		// ID or key is still missing, try again later
		return;
	}

	// Check the pairing:
	auto pairings = mComponents.get<DevicePairings>();
	auto pairing = pairings->lookupDevice(mRemotePublicID.value());
	if (!pairing.isPresent())
	{
		// We don't have a record of this pairing, but the device does. Consider unpaired:
		setState(csUnknownPairing);
		emit unknownPairing(this);
	}
	else if (pairing.value().mDevicePublicKeyData.isEmpty())
	{
		// We've just decided to pair to this device, so we have our keypair, but haven't received the device's yet
		// Consider unpaired yet:
		setState(csUnknownPairing);
		emit unknownPairing(this);
	}
	else if (pairing.value().mDevicePublicKeyData == mRemotePublicKeyData.value())
	{
		// Pairing matches, continue to TLS:
		setState(csKnownPairing);
		emit knownPairing(this);
		if (!mHasSentStartTls)
		{
			sendCleartextMessage("stls"_4cc);
		}
	}
	else
	{
		// Pairing with a different key, MITM attack? Let someone up decide:
		setState(csDifferentKey);
		emit differentKey(this);
	}
}





void Connection::setState(Connection::State aNewState)
{
	mState = aNewState;
	emit stateChanged(this, aNewState);
}





void Connection::processIncomingData(const QByteArray & aData)
{
	switch (mState)
	{
		case csInitial:
		case csBlacklisted:
		case csDifferentKey:
		case csKnownPairing:
		case csUnknownPairing:
		case csRequestedPairing:
		{
			// Add the data to the internal buffer:
			mIncomingData.append(aData);

			// Extract all complete messages:
			while (extractAndHandleCleartextMessage())
			{
				// Empty loop body
			}
			break;
		}

		case csEncrypted:
		{
			// Add the data to the internal buffer, then process the TLS protocol:
			mIncomingDataEncrypted.append(aData);
			processTls();
			break;
		}

		case csDisconnected:
		{
			// Probably some leftover data from the connection, don't know how to handle it, so drop it:
			mLogger.logHex(aData, "Received data while disconnected, ignoring.");
			break;
		}
	}
}





bool Connection::extractAndHandleCleartextMessage()
{
	// Check if there are enough bytes for the entire message:
	if (mIncomingData.size() < 6)
	{
		return false;
	}
	auto bytes = reinterpret_cast<const quint8 *>(mIncomingData.constData());
	quint16 msgLen = Utils::readBE16(&(bytes[4]));
	if (mIncomingData.size() < msgLen + 6)
	{
		return false;
	}

	// Extract and handle the message:
	auto msgType = Utils::readBE32(bytes);
	auto msg = mIncomingData.mid(6, msgLen);
	mIncomingData.remove(0, msgLen + 6);
	handleCleartextMessage(msgType, msg);
	return true;
}





void Connection::handleCleartextMessage(const quint32 aMsgType, const QByteArray & aMsg)
{
	mLogger.log("Received cleartext message \"%1\".", Utils::writeBE32(aMsgType));

	if (!mHasReceivedIdentification && (aMsgType != "dsms"_4cc))
	{
		mLogger.log("Didn't receive an identification message first; instead got \"%1\".", Utils::writeBE32(aMsgType));
		terminate();
		return;
	}
	switch (aMsgType)
	{
		case "dsms"_4cc:
		{
			handleCleartextMessageDsms(aMsg);
			break;
		}
		case "fnam"_4cc:
		{
			setFriendlyName(QString::fromUtf8(aMsg));
			break;
		}
		case "avtr"_4cc:
		{
			setAvatar(aMsg);
			break;
		}
		case "pubi"_4cc:
		{
			setRemotePublicID(aMsg);
			break;
		}
		case "pubk"_4cc:
		{
			setRemotePublicKey(aMsg);
			break;
		}
		case "pair"_4cc:
		{
			requestPairing();
			break;
		}
		case "stls"_4cc:
		{
			handleCleartextMessageStls();
			break;
		}
		default:
		{
			mLogger.log("Unknown message received: \"%1\".", Utils::writeBE32(aMsgType));
			terminate();
			break;
		}
	}
}





void Connection::handleCleartextMessageDsms(const QByteArray & aMsg)
{
	if (aMsg.size() < 10)
	{
		mLogger.log("ERROR: Received a too short protocol identification: %1 bytes.", aMsg.size());
		terminate();
		return;
	}
	if (!aMsg.startsWith("Deskemes"))
	{
		mLogger.log("ERROR: Received an invalid protocol identification");
		terminate();
		return;
	}
	mRemoteVersion = Utils::readBE16(aMsg, 8);
	mHasReceivedIdentification = true;

	// Send our public data:
	auto ic = mComponents.get<InstallConfiguration>();
	sendCleartextMessage("pubi"_4cc, ic->publicID());
	sendCleartextMessage("fnam"_4cc, ic->friendlyName().toUtf8());
	const auto & avatar = ic->avatar();
	if (avatar.isPresent())
	{
		sendCleartextMessage("avtr"_4cc, avatar.value());
	}
}





void Connection::handleCleartextMessageStls()
{
	mLogger.log("Received a TLS request");

	// Check if we have all the information needed
	if (
		!mRemotePublicID.isPresent() ||
		!mRemotePublicKeyData.isPresent()
	)
	{
		mLogger.log("ERROR: Remote asked for TLS, but hasn't provided PublicID or PublicKey, aborting connection");
		terminate();
		return;
	}

	// Check our pairing for the remote:
	auto pairings = mComponents.get<DevicePairings>();
	auto pairing = pairings->lookupDevice(mRemotePublicID.value());
	if (!pairing.isPresent())
	{
		mLogger.log("Remote requested TLS, but we don't have a pairing for it");
		return;
	}

	// If we didn't send the StartTls yet, do it now:
	if (!mHasSentStartTls)
	{
		sendCleartextMessage("stls"_4cc);
	}

	mLogger.log("Received a TLS request, upgrading to TLS");
	mTls = std::make_unique<CallbackSslContext>(*this);
	auto config = SslConfig::makeDefaultConfig(false);
	auto privKey = std::make_shared<CryptoKey>(pairing.value().mLocalPrivateKeyData.toStdString(), std::string());
	auto pubKey = std::make_shared<CryptoKey>(pairing.value().mLocalPublicKeyData.toStdString());
	auto cert = X509Cert::fromPrivateKey(privKey, pubKey, "CN=Deskemes");
	config->setOwnCert(cert, privKey);
	config->setAuthMode(SslAuthMode::Optional);
	config->setVerifyCallback(tlsVerifyCallback, this);
	#ifdef DEBUG_TLS
		mbedtls_debug_set_threshold(DEBUG_TLS);
		config->setDebugCallback(tlsDebugCallback, this);
	#endif  //
	mTls->initialize(config);
	setState(csEncrypted);
	std::swap(mIncomingData, mIncomingDataEncrypted);  // All the data received from the socket after the "stls" is expected to be TLSed
	processTls();

	// Set up the command channel:
	auto ch0 = std::make_shared<ChannelZero>(*this);
	mChannels[0] = ch0;
	connect(ch0.get(), &ChannelZero::channelAcknowledged, ch0.get(),
		[this](ChannelPtr aChannel)
		{
			assert(aChannel != nullptr);
			QMutexLocker lock(&mMtxChannels);
			assert(mChannels.find(aChannel->channelID()) == mChannels.end());
			mChannels[aChannel->channelID()] = aChannel;
		}
	);

	// Push the leftover data to the TLS filter:
	std::swap(mIncomingDataEncrypted, mIncomingData);
	mTls->performHandshake();

	emit established(this);
}





void Connection::sendCleartextMessage(quint32 aMsgType, const QByteArray & aMsg)
{
	if (mHasSentStartTls)
	{
		mLogger.log("ERROR: Trying to send cleartext message \"%1\" when TLS start has already been requested from this side.", Utils::writeBE32(aMsgType));
		assert(!"TLS start has already been requested");
		return;
	}

	QByteArray packet = Utils::writeBE32(aMsgType);
	Utils::writeBE16Lstring(packet, aMsg);
	mLogger.logHex(packet, "Outoing cleartext message (%1; %2 bytes)", Utils::writeBE32(aMsgType), aMsg.length());
	mIO->write(packet);
	if (aMsgType == "stls"_4cc)
	{
		mHasSentStartTls = true;
	}
}





bool Connection::extractAndHandleMuxMessage()
{
	if (mIncomingData.size() < 4)
	{
		// Not enough data for the header
		return false;
	}
	auto msgLen = static_cast<qint32>(Utils::readBE16(mIncomingData, 2));
	if (mIncomingData.size() < msgLen + 4)
	{
		return false;
	}
	auto channelID = Utils::readBE16(mIncomingData, 0);
	auto channel = channelByID(channelID);
	if (channel == nullptr)
	{
		mLogger.log("Message received for non-existent channel %1.", channelID);
		return true;
	}
	mLogger.logHex(mIncomingData.mid(4, msgLen), "Incoming message (%1 bytes) for channel %2", msgLen, channelID);
	channel->processIncomingMessage(mIncomingData.mid(4, msgLen));
	mIncomingData = mIncomingData.mid(4 + msgLen);
	return true;
}





Connection::ChannelPtr Connection::channelByID(quint16 aChannelID)
{
	QMutexLocker lock(&mMtxChannels);
	auto itr = mChannels.find(aChannelID);
	if (itr == mChannels.end())
	{
		return nullptr;
	}
	return itr->second;
}





std::shared_ptr<ChannelZero> Connection::channelZero()
{
	return std::static_pointer_cast<ChannelZero>(channelByID(0));
}





void Connection::addChannel(const quint16 aChannelID, Connection::ChannelPtr aChannel)
{
	QMutexLocker lock(&mMtxChannels);
	assert(mChannels[aChannelID] == nullptr);
	mChannels[aChannelID] = aChannel;
}





void Connection::sendChannelMessage(const quint16 aChannelID, const QByteArray & aMessage)
{
	mLogger.logHex(aMessage, "Sending message to channel #%1", aChannelID);
	assert(mState == csEncrypted);
	assert(mTls != nullptr);
	Utils::writeBE16(mOutgoingDataPlain, aChannelID);
	Utils::writeBE16Lstring(mOutgoingDataPlain, aMessage);
	processTls();
}





void Connection::processTls()
{
	assert(mState == csEncrypted);
	assert(mTls != nullptr);

	bool shouldLoop = true;
	while (shouldLoop)
	{
		shouldLoop = false;

		// Write the outgoing data, if possible:
		if (!mOutgoingDataPlain.isEmpty())
		{
			auto numWritten = mTls->writePlain(mOutgoingDataPlain.constData(), static_cast<size_t>(mOutgoingDataPlain.size()));
			if (numWritten > 0)
			{
				mOutgoingDataPlain.remove(0, numWritten);
				shouldLoop = true;
			}
			else if (
				(numWritten != MBEDTLS_ERR_SSL_WANT_READ) &&
				(numWritten != MBEDTLS_ERR_SSL_WANT_WRITE)
			)
			{
				mLogger.log("ERROR: TLS writing failed: -0x%1", QString::number(-numWritten, 16));
				terminate();
			}
		}

		// Read the incoming data, if possible:
		char buffer[3000];
		auto numRead = mTls->readPlain(buffer, sizeof(buffer));
		if (numRead > 0)
		{
			mIncomingData.append(buffer, numRead);
			while (extractAndHandleMuxMessage())
			{
				// Empty loop body
			}
			shouldLoop = true;
		}
		else if (
			(numRead != MBEDTLS_ERR_SSL_WANT_READ) &&
			(numRead != MBEDTLS_ERR_SSL_WANT_WRITE)
		)
		{
			mLogger.log("ERROR: TLS reading failed: -0x%1 (%2)", QString::number(-numRead, 16), TlsException::mbedTlsCodeToString(numRead));
			terminate();
		}
	}
}





int Connection::receiveEncrypted(unsigned char * aBuffer, size_t aNumBytes)
{
	if (mIncomingDataEncrypted.isEmpty())
	{
		return MBEDTLS_ERR_SSL_WANT_READ;
	}
	int numBytes = std::min(static_cast<int>(aNumBytes), mIncomingDataEncrypted.size());
	memcpy(aBuffer, mIncomingDataEncrypted.constData(), static_cast<size_t>(numBytes));
	mIncomingDataEncrypted.remove(0, numBytes);
	return numBytes;
}





int Connection::sendEncrypted(const unsigned char * aBuffer, size_t aNumBytes)
{
	return static_cast<int>(mIO->write(reinterpret_cast<const char *>(aBuffer), static_cast<qint64>(aNumBytes)));
}





void Connection::ioReadyRead()
{
	while (true)
	{
		char buf[4000];
		auto numBytes = mIO->read(buf, sizeof(buf));
		if (numBytes == 0)
		{
			return;
		}
		else if (numBytes < 0)
		{
			mLogger.log("ERROR: Reading from IO failed: %1", numBytes);
			ioClosing();
			return;
		}
		processIncomingData(QByteArray::fromRawData(buf, static_cast<int>(numBytes)));
	}
}





void Connection::ioClosing()
{
	mLogger.log("Disconnected");
	setState(csDisconnected);
	emit disconnected(this);

	// Clear all channels only after notifying "disconnected", in case client has some data in the channels
	mChannels.clear();
}
