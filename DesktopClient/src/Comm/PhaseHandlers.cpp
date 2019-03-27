#include "PhaseHandlers.hpp"
#include <cassert>
#include <QDebug>
#include "../Utils.hpp"
#include "../ComponentCollection.hpp"
#include "../InstallConfiguration.hpp"
#include "Connection.hpp"





////////////////////////////////////////////////////////////////////////////////
// PhaseHandler:

PhaseHandler::PhaseHandler(Connection & aConnection):
	mConnection(aConnection)
{
}





void PhaseHandler::write(const QByteArray & aData)
{
	mConnection.write(aData);
}





////////////////////////////////////////////////////////////////////////////////
// PhaseHandlerMsgBase:

void PhaseHandlerMsgBase::processIncomingData(const QByteArray & aData)
{
	// Add the data to the internal buffer:
	mIncomingData.append(aData);

	// Extract all complete messages:
	while (extractAndHandleMessage())
	{
		// Empty loop body
	}

	mOldPhaseHandler.reset();
}





void PhaseHandlerMsgBase::abortConnection()
{
	mConnection.terminate();
	mIncomingData.clear();
}





////////////////////////////////////////////////////////////////////////////////
// PhaseHandlerCleartext:

PhaseHandlerCleartext::PhaseHandlerCleartext(
	ComponentCollection & aComponents,
	Connection & aConnection
):
	Super(aConnection),
	mComponents(aComponents),
	mHasReceivedIdentification(false),
	mRemoteVersion(0),
	mHasSentStartTls(false)
{
	// Send the protocol identification:
	qDebug() << "Sending protocol identification";
	QByteArray protocolIdent("Deskemes");
	Utils::writeBE16(protocolIdent, 1);  // version
	sendMessage("dsms"_4cc, protocolIdent);
}





bool PhaseHandlerCleartext::extractAndHandleMessage()
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
	handleMessage(msgType, msg);
	return true;
}





void PhaseHandlerCleartext::handleMessage(const quint32 aMsgType, const QByteArray & aMsg)
{
	if (!mHasReceivedIdentification && (aMsgType != "dsms"_4cc))
	{
		qDebug() << "Didn't receive an identification message first; instead got " << Utils::writeBE32(aMsgType);
		abortConnection();
		return;
	}
	switch (aMsgType)
	{
		case "dsms"_4cc:
		{
			handleMessageDsms(aMsg);
			break;
		}
		case "fnam"_4cc:
		{
			mConnection.setFriendlyName(QString::fromUtf8(aMsg));
			break;
		}
		case "avtr"_4cc:
		{
			mConnection.setAvatar(aMsg);
			break;
		}
		case "pubi"_4cc:
		{
			mConnection.setRemotePublicID(aMsg);
			break;
		}
		case "pubk"_4cc:
		{
			handleMessagePubk(aMsg);
			break;
		}
		case "pair"_4cc:
		{
			mConnection.requestPairing();
			break;
		}
		case "stls"_4cc:
		{
			handleMessageStls();
			break;
		}
		default:
		{
			qDebug() << "Unknown message received: " << Utils::writeBE32(aMsgType);
			abortConnection();
			break;
		}
	}
}





void PhaseHandlerCleartext::handleMessageDsms(const QByteArray & aMsg)
{
	if (aMsg.size() < 10)
	{
		qDebug() << "Received a too short protocol identification: " << aMsg.size() << " bytes";
		abortConnection();
		return;
	}
	if (!aMsg.startsWith("Deskemes"))
	{
		qDebug() << "Received an invalid protocol identification";
		abortConnection();
		return;
	}
	mRemoteVersion = Utils::readBE16(aMsg, 8);
	mHasReceivedIdentification = true;

	// Send our public data:
	auto ic = mComponents.get<InstallConfiguration>();
	sendMessage("pubi"_4cc, ic->publicID());
	sendMessage("fnam"_4cc, ic->friendlyName().toUtf8());
	const auto & avatar = ic->avatar();
	if (avatar.isPresent())
	{
		sendMessage("avtr"_4cc, avatar.value());
	}
}





void PhaseHandlerCleartext::handleMessagePubk(const QByteArray & aMsg)
{
	mConnection.setRemotePublicKey(aMsg);
}





void PhaseHandlerCleartext::handleMessageStls()
{
	// TODO: Check if we have all the information needed

	// If we didn't send the StartTls yet, do it now:
	if (!mHasSentStartTls)
	{
		sendMessage("stls"_4cc);
	}
	qDebug() << "Received a TLS request, upgrading to TLS";

	// TODO: Add an actual TLS filter

	auto newHandler = std::make_unique<PhaseHandlerMuxer>(mConnection);
	newHandler->processIncomingData(mIncomingData);
	mIncomingData.clear();
	mOldPhaseHandler = mConnection.setPhaseHandler(std::move(newHandler), Connection::csEncrypted);
}





void PhaseHandlerCleartext::sendMessage(quint32 aMsgType, const QByteArray & aMsg)
{
	if (mHasSentStartTls)
	{
		qWarning() << "Trying to send message " << Utils::writeBE32(aMsgType) << " when TLS start has already been requested.";
		assert(!"TLS start has already been requested");
		return;
	}

	QByteArray packet = Utils::writeBE32(aMsgType);
	Utils::writeBE16Lstring(packet, aMsg);
	mConnection.write(packet);
	if (aMsgType == "stls"_4cc)
	{
		mHasSentStartTls = true;
	}
}





////////////////////////////////////////////////////////////////////////////////
// PhaseHandlerMuxer:

PhaseHandlerMuxer::PhaseHandlerMuxer(Connection & aConnection):
	Super(aConnection)
{
}





bool PhaseHandlerMuxer::extractAndHandleMessage()
{
	if (mIncomingData.size() < 4)
	{
		// Not enough data even for an empty packet
		return false;
	}

	// TODO

	return false;
}
