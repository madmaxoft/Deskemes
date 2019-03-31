#include "Connection.hpp"
#include <cassert>
#include "PhaseHandlers.hpp"
#include "../DB/DevicePairings.hpp"





Connection::Connection(
	ComponentCollection & aComponents,
	QIODevice * aIO,
	TransportKind aTransportKind,
	const QString & aTransportName,
	QObject * aParent
):
	Super(aParent),
	mComponents(aComponents),
	mIO(aIO),
	mTransportKind(aTransportKind),
	mTransportName(aTransportName),
	mPhaseHandler(std::make_unique<PhaseHandlerCleartext>(aComponents, *this)),
	mState(csInitial)
{
	connect(aIO, &QIODevice::readyRead,    this, &Connection::ioReadyRead);
	connect(aIO, &QIODevice::aboutToClose, this, &Connection::ioClosing);

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





void Connection::write(const QByteArray & aData)
{
	mIO->write(aData);
}





void Connection::requestPairing()
{
	assert(mState == csInitial);
	setState(csRequestedPairing);
	emit requestingPairing(this);
}





std::unique_ptr<PhaseHandler> Connection::setPhaseHandler(
	std::unique_ptr<PhaseHandler> && aNewHandler,
	Connection::State aNewState
)
{
	std::swap(aNewHandler, mPhaseHandler);
	setState(aNewState);
	return std::move(aNewHandler);
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
		// TODO
	}

	checkRemotePublicKeyAndID();
}





void Connection::setRemotePublicKey(const QByteArray & aPubKeyData)
{
	mRemotePublicKeyData = aPubKeyData;
	emit receivedPublicKey(this);
	checkRemotePublicKeyAndID();
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
		setState(csUnknownPairing);
		emit unknownPairing(this);
	}
	else if (pairing.value().mDevicePublicKeyData == mRemotePublicKeyData.value())
	{
		setState(csKnownPairing);
		emit knownPairing(this);
	}
	else
	{
		setState(csDifferentKey);
		emit differentKey(this);
	}
}





void Connection::setState(Connection::State aNewState)
{
	mState = aNewState;
	emit stateChanged(aNewState);
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
		mPhaseHandler->processIncomingData(data);
	}
}





void Connection::ioClosing()
{
	emit disconnected(this);
}
