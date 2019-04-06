#include "Connection.hpp"
#include <cassert>
#include "../InstallConfiguration.hpp"
#include "../Utils.hpp"
#include "../DB/DevicePairings.hpp"





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
	connect(aIO, &QIODevice::readyRead,    this, &Connection::ioReadyRead);
	connect(aIO, &QIODevice::aboutToClose, this, &Connection::ioClosing);

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
			// TODO: TLS Mux
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





void Connection::ioReadyRead()
{
	while (true)
	{
		auto data = mIO->read(4000);
		if (data.size() == 0)
		{
			return;
		}
		processIncomingData(data);
	}
}





void Connection::ioClosing()
{
	emit disconnected(this);
}
