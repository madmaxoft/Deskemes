#include "PgPairInit.hpp"
#include <cassert>
#include "ui_PgPairInit.h"
#include "../../Utils.hpp"
#include "../../BackgroundTasks.hpp"
#include "../../Comm/Connection.hpp"
#include "../../DB/DevicePairings.hpp"
#include "../NewDeviceWizard.hpp"





PgPairInit::PgPairInit(ComponentCollection & aComponents, NewDeviceWizard & aParent):
	Super(&aParent),
	mComponents(aComponents),
	mParent(aParent),
	mUI(new Ui::PgPairInit),
	mHasReceivedRemotePublicKey(false),
	mIsLocalKeyPairCreated(false)
{
	mUI->setupUi(this);
	setTitle(tr("Waiting for device"));
	setSubTitle(tr("Initializing a secure connection to the device"));
}





PgPairInit::~PgPairInit()
{
	// Nothing explicit needed yet
}





void PgPairInit::initializePage()
{
	auto conn = mParent.connection();
	assert(conn->remotePublicID().isPresent());
	connect(conn.get(), &Connection::receivedPublicKey, this, &PgPairInit::receivedPublicKey);
	auto displayName = conn->friendlyName().valueOr(Utils::toHex(conn->remotePublicID().value()));
	auto pairings = mComponents.get<DevicePairings>();
	mIsLocalKeyPairCreated = pairings->lookupDevice(conn->remotePublicID().value()).isPresent();
	if (!mIsLocalKeyPairCreated)
	{
		qDebug() << "Enqueueing local public key generation";
		BackgroundTasks::enqueue(
			tr("Generate public key for %1").arg(displayName),
			[this, conn, displayName, pairings]()
			{
				qDebug() << "Sending local public key to " << displayName;
				pairings->createLocalKeyPair(conn->remotePublicID().value(), displayName);
				QMetaObject::invokeMethod(conn.get(), "sendLocalPublicKey");
				QMetaObject::invokeMethod(this, "localKeyPairCreated");
				QMetaObject::invokeMethod(conn.get(), "sendPairingRequest");
			}
		);
	}
	else
	{
		conn->sendPairingRequest();
	}
	if (conn->remotePublicKeyData().isPresent())
	{
		// Need to queue-invoke this method, otherwise the wizard becomes confused about what page to display
		qDebug() << "The remote public key has already been received, invoking receivedPublicKey()";
		QMetaObject::invokeMethod(this, "receivedPublicKey", Qt::QueuedConnection, Q_ARG(Connection *, conn.get()));
	}
}





int PgPairInit::nextId() const
{
	return NewDeviceWizard::pgPairConfirm;
}





bool PgPairInit::isComplete() const
{
	qDebug() << "IsComplete: " << mHasReceivedRemotePublicKey << ", " << mIsLocalKeyPairCreated;
	return mHasReceivedRemotePublicKey && mIsLocalKeyPairCreated;
}





void PgPairInit::receivedPublicKey(Connection * aConnection)
{
	mHasReceivedRemotePublicKey = (mParent.connection()->remotePublicKeyData().isPresent());
	if (!mHasReceivedRemotePublicKey)
	{
		// This was called for a different connection? Perhaps going back and forth in the wizard.
		assert(aConnection != mParent.connection().get());
		return;
	}
	qDebug() << "Received device's public key";
	completeChanged();
	if (isComplete())
	{
		mParent.next();
	}
}





void PgPairInit::localKeyPairCreated()
{
	qDebug() << "Local keypair has been generated";
	mIsLocalKeyPairCreated = true;
	completeChanged();
	if (isComplete())
	{
		mParent.next();
	}
}
