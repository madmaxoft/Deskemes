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

	// Check if the state was changed before we even started monitoring:
	// Need to queue-invoke this method, otherwise the wizard becomes confused about what page to display
	QMetaObject::invokeMethod(this, "connStateChanged", Qt::QueuedConnection, Q_ARG(Connection *, conn.get()));
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
