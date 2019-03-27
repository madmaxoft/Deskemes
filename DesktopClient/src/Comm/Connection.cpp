#include "Connection.hpp"
#include <cassert>
#include "PhaseHandlers.hpp"





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
	connect(aIO, &QIODevice::readyRead, this, &Connection::ioReadyRead);

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
	mState = csRequestedPairing;
	emit requestingPairing();
}





std::unique_ptr<PhaseHandler> Connection::setPhaseHandler(
	std::unique_ptr<PhaseHandler> && aNewHandler,
	Connection::State aNewState
)
{
	std::swap(aNewHandler, mPhaseHandler);
	mState = aNewState;
	return std::move(aNewHandler);
}





void Connection::setRemotePublicID(const QByteArray & aPublicID)
{
	mRemotePublicID = aPublicID;
	checkRemotePublicKeyAndID();
}





void Connection::setRemotePublicKey(const QByteArray & aPubKeyData)
{
	mRemotePublicKeyData = aPubKeyData;
	checkRemotePublicKeyAndID();
}





void Connection::checkRemotePublicKeyAndID()
{
	// TODO: Check the pairing
	mState = csNeedPairing;
	emit unknownPairing();
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
