#include "PgPairInit.hpp"
#include <cassert>
#include "ui_PgPairInit.h"
#include "../../Comm/Connection.hpp"
#include "../NewDeviceWizard.hpp"
#include "../../BackgroundTasks.hpp"
#include "../../DB/DevicePairings.hpp"





PgPairInit::PgPairInit(ComponentCollection & aComponents, NewDeviceWizard & aParent):
	Super(&aParent),
	mComponents(aComponents),
	mParent(aParent),
	mUI(new Ui::PgPairInit),
	mIsComplete(false)
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
	auto displayName = conn->friendlyName().valueOr(QString::fromUtf8(conn->remotePublicID().value()));
	BackgroundTasks::enqueue(
		tr("Generate public key for %1").arg(displayName),
		[conn, displayName, pairings = mComponents.get<DevicePairings>()]()
		{
			qDebug() << "Sending local public key to " << displayName;
			pairings->createLocalKeyPair(conn->remotePublicID().value());
			QMetaObject::invokeMethod(conn.get(), "sendLocalPublicKey");
		}
	);
	if (conn->remotePublicKeyData().isPresent())
	{
		receivedPublicKey(conn.get());
	}

}





int PgPairInit::nextId() const
{
	return NewDeviceWizard::pgPairConfirm;
}





bool PgPairInit::isComplete() const
{
	return mIsComplete;
}





void PgPairInit::receivedPublicKey(Connection * aConnection)
{
	mIsComplete = (mParent.connection()->remotePublicKeyData().isPresent());
	if (!mIsComplete)
	{
		// This was called for a different connection? Perhaps going back and forth in the wizard.
		assert(aConnection != mParent.connection().get());
		return;
	}
	qDebug() << "Received device's public key";
	completeChanged();
	mParent.next();
}
