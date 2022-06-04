#include "NewDeviceWizard.hpp"
#include <QWizard>
#include "Pages/PgDeviceList.hpp"
#include "Pages/PgNeedApp.hpp"
#include "Pages/PgNeedAuth.hpp"
#include "Pages/PgPairInit.hpp"
#include "Pages/PgPairConfirm.hpp"
#include "Pages/PgPairingInProgress.hpp"
#include "Pages/PgSucceeded.hpp"
#include "../Comm/UdpBroadcaster.hpp"





NewDeviceWizard::NewDeviceWizard(ComponentCollection & aComponents, QWidget * aParent):
	Super(aParent),
	mComponents(aComponents),
	mLogger(aComponents.logger("DetectedDevices"), "NewDeviceWizard: ")  // Use the same logger as DetectedDevices
{
	setWindowTitle(tr("Deskemes: Add new device"));
	setWizardStyle(QWizard::ModernStyle);
	setPage(pgDeviceList,        new PgDeviceList       (mComponents, *this));
	setPage(pgNeedApp,           new PgNeedApp          (mComponents, *this));
	setPage(pgNeedAuth,          new PgNeedAuth         (mComponents, *this));
	setPage(pgPairInit,          new PgPairInit         (mComponents, *this));
	setPage(pgPairConfirm,       new PgPairConfirm      (mComponents, *this));
	setPage(pgPairingInProgress, new PgPairingInProgress(mComponents, *this));
	setPage(pgSucceeded,         new PgSucceeded        (mComponents, *this));
	setStartId(pgDeviceList);

	// Start explicit discovery:
	mComponents.get<UdpBroadcaster>()->startDiscovery();
	mLogger.log("Created the wizard");

	connect(this, &QWizard::currentIdChanged, this,
		[=](int aID)
		{
			mLogger.log("Going to page %1 (%2)", aID, pageIdToString(aID));
		}
	);
}





NewDeviceWizard::~NewDeviceWizard()
{
	mLogger.log("The wizard is terminating.");
	mComponents.get<UdpBroadcaster>()->endDiscovery();
}





void NewDeviceWizard::setDevice(DetectedDevices::DevicePtr aDevice)
{
	mLogger.log("Setting the device to %1", aDevice->enumeratorDeviceID());
	mDevice = aDevice;
}





void NewDeviceWizard::setConnection(ConnectionPtr aConnection)
{
	mLogger.log("Setting the connection to %1", aConnection->connectionID());
	mConnection = aConnection;
}





QString NewDeviceWizard::pageIdToString(int aPageID)
{
	switch (aPageID)
	{
		case -1:                  return "<Invalid page>";  // Used internally by Qt when cancelling the wizard
		case pgDeviceList:        return "pgDeviceList";
		case pgBlacklisted:       return "pgBlacklisted";
		case pgNeedAuth:          return "pgNeedAuth";
		case pgNeedApp:           return "pgNeedApp";
		case pgPairInit:          return "pgPairInit";
		case pgPairConfirm:       return "pgPairConfirm";
		case pgPairingInProgress: return "pgPairingInProgress";
		case pgFailed:            return "pgFailed";
		case pgSucceeded:         return "pgSucceeded";
	}
	assert(!"Unknown page ID");
	return "<unknown page ID>";
}
