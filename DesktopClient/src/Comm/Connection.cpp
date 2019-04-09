#include "Connection.hpp"
#include <cassert>
#include <QTimer>
#include <QDateTime>
#include "../InstallConfiguration.hpp"
#include "../Utils.hpp"
#include "../DB/DevicePairings.hpp"





////////////////////////////////////////////////////////////////////////////////
// ChannelZero:

/** The implementation of the special channel zero - the one that manages all the other channels. */
class ChannelZero:
	public Connection::Channel
{
	using Super = Connection::Channel;

public:

	ChannelZero(Connection & aConnection, quint16 aID):
		Super(aConnection, aID)
	{
		// Add a ping-timer
		connect(&mTimer, &QTimer::timeout, this, &ChannelZero::sendPing);
		mTimer.start(1000);  // 1 second
	}


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
		mConnection.sendChannelMessage(this, buf);
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
		mConnection.sendChannelMessage(this, buf);
	}





	/** Sends a request and calls the specified handler when response or error is received.
	Automatically assigns a request ID.
	The error handler is called immediately if there's no free request IDs, with ERR_NO_REQUEST_ID. */
	void sendRequest(
		const quint32 aRequestType,
		PendingRequest::SuccessHandler aSuccessHandler,
		std::function<void(const quint16 aErrorCode, const QByteArray & aErrorMsg)> aErrorHandler,
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
		mConnection.sendChannelMessage(this, req);
	}





	/** Called when a ping's response is received, with the calculated roundtrip time. */
	void pingReceived(qint64 aRoundtripMsec)
	{
		qDebug() << "Ping received in " << aRoundtripMsec << " msec";
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
			[](const quint16 aErrorCode, const QByteArray & aErrorMessage)
			{
				qDebug() << "Ping request returned an error: " << aErrorCode << "; " << aErrorMessage;
			},
			QString::number(numPings++).toUtf8()
		);
	}


protected:

	/** The timer used for pinging. */
	QTimer mTimer;
};





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
	mHasSentStartTls(false)
{
	connect(aIO, &QIODevice::readyRead,           this, &Connection::ioReadyRead);
	connect(aIO, &QIODevice::aboutToClose,        this, &Connection::ioClosing);
	connect(aIO, &QIODevice::readChannelFinished, this, &Connection::ioClosing);

	// Send the protocol identification:
	qDebug() << "Sending protocol identification";
	QByteArray protocolIdent("Deskemes");
	Utils::writeBE16(protocolIdent, 1);  // version
	sendCleartextMessage("dsms"_4cc, protocolIdent);

	// If there's data already on the connection, process it:
	if (aIO->waitForReadyRead(1))
	{
		ioReadyRead();
	}
}





void Connection::terminate()
{
	mIO->close();
}





void Connection::requestPairing()
{
	assert(mState == csInitial);
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

	qDebug() << "Sending public key data";
	sendCleartextMessage("pubk"_4cc, pairing.value().mLocalPublicKeyData);
}





void Connection::localPairingApproved()
{
	assert(
		(mState == csUnknownPairing) ||
		(mState == csKnownPairing) ||
		(mState == csRequestedPairing) ||
		(mState == csDifferentKey)
	);
	assert(!mHasSentStartTls);
	assert(mRemotePublicID.isPresent());
	assert(mRemotePublicKeyData.isPresent());
	const auto & id = mRemotePublicID.value();
	auto pairings = mComponents.get<DevicePairings>();
	auto pairing = pairings->lookupDevice(id);
	assert(pairing.isPresent());
	assert(!pairing.value().mLocalPublicKeyData.isEmpty());
	assert(!pairing.value().mLocalPrivateKeyData.isEmpty());

	pairings->pairDevice(
		id,
		mRemotePublicKeyData.value(),
		pairing.value().mLocalPublicKeyData,
		pairing.value().mLocalPrivateKeyData
	);

	sendCleartextMessage("stls"_4cc);
}





void Connection::sendChannelMessage(const Connection::Channel * aChannel, const QByteArray & aMessage)
{
	QByteArray buf;
	Utils::writeBE16(buf, aChannel->channelID());
	Utils::writeBE16Lstring(buf, aMessage);

	// TODO: TLS

	mIO->write(buf);
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
		sendCleartextMessage("stls"_4cc);
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
			// TODO: TLS

			// Add the data to the internal buffer:
			mIncomingData.append(aData);

			// Extract all complete messages:
			while (extractAndHandleMuxMessage())
			{
				// Empty loop body
			}
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
	if (!mHasReceivedIdentification && (aMsgType != "dsms"_4cc))
	{
		qDebug() << "Didn't receive an identification message first; instead got " << Utils::writeBE32(aMsgType);
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
			qDebug() << "Unknown message received: " << Utils::writeBE32(aMsgType);
			terminate();
			break;
		}
	}
}





void Connection::handleCleartextMessageDsms(const QByteArray & aMsg)
{
	if (aMsg.size() < 10)
	{
		qDebug() << "Received a too short protocol identification: " << aMsg.size() << " bytes";
		terminate();
		return;
	}
	if (!aMsg.startsWith("Deskemes"))
	{
		qDebug() << "Received an invalid protocol identification";
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
	// Check if we have all the information needed
	if (
		!mRemotePublicID.isPresent() ||
		!mRemotePublicKeyData.isPresent()
	)
	{
		qDebug() << "Remote asked for TLS, but hasn't provided PublicID or PublicKey, aborting connection";
		terminate();
		return;
	}

	// If we didn't send the StartTls yet, do it now:
	if (!mHasSentStartTls)
	{
		sendCleartextMessage("stls"_4cc);
	}
	qDebug() << "Received a TLS request, upgrading to TLS";

	// TODO: Add an actual TLS filter

	setState(csEncrypted);

	// Set up the command channel:
	mChannels[0] = std::make_shared<ChannelZero>(*this, 0);

	// Push the leftover data to the TLS filter:
	QByteArray leftoverData;
	std::swap(leftoverData, mIncomingData);
	processIncomingData(leftoverData);
}





void Connection::sendCleartextMessage(quint32 aMsgType, const QByteArray & aMsg)
{
	if (mHasSentStartTls)
	{
		qWarning() << "Trying to send message " << Utils::writeBE32(aMsgType) << " when TLS start has already been requested.";
		assert(!"TLS start has already been requested");
		return;
	}

	QByteArray packet = Utils::writeBE32(aMsgType);
	Utils::writeBE16Lstring(packet, aMsg);
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
		qWarning() << "Message received for non-existent channel " << channelID;
		return true;
	}
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
			qDebug() << "Reading from IO failed";
			ioClosing();
			return;
		}
		processIncomingData(QByteArray::fromRawData(buf, static_cast<int>(numBytes)));
	}
}





void Connection::ioClosing()
{
	qDebug() << "Disconnected";
	emit disconnected(this);
}
