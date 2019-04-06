#include "PgPairingInProgress.hpp"
#include <cassert>
#include "ui_PgPairingInProgress.h"
#include "../../ComponentCollection.hpp"
#include "../../Comm/Connection.hpp"
#include "../NewDeviceWizard.hpp"





PgPairingInProgress::PgPairingInProgress(ComponentCollection & aComponents, NewDeviceWizard & aParent):
	Super(&aParent),
	mComponents(aComponents),
	mParent(aParent),
	mUI(new Ui::PgPairingInProgress)
{
	mUI->setupUi(this);
	setTitle(tr("Waiting for confirmation"));
	setSubTitle(tr("Please confirm the pairing on your device"));
}





PgPairingInProgress::~PgPairingInProgress()
{
	// Nothing explicit needed yet
}





void PgPairingInProgress::initializePage()
{
	auto conn = mParent.connection();
	assert(conn != nullptr);
	connect(conn.get(), &Connection::stateChanged, this, &PgPairingInProgress::connStateChanged);
}





void PgPairingInProgress::connStateChanged(Connection * aConnection)
{
	if (aConnection != mParent.connection().get())
	{
		qWarning() << "Different connection reporting";
		return;
	}
	switch (aConnection->state())
	{
		case Connection::csEncrypted:
		{
			// Success!
			mNextPage = NewDeviceWizard::pgSucceeded;
			emit completeChanged();
			mParent.next();
			return;
		}
		case Connection::csRequestedPairing:
		case Connection::csKnownPairing:
		{
			// Still waiting
			mNextPage = -1;
			emit completeChanged();
			return;
		}
		default:
		{
			// Error
			mNextPage = NewDeviceWizard::pgFailed;
			emit completeChanged();
			mParent.next();
			return;
		}
	}
}
