#include "NewDeviceWizard.hpp"
#include <QWizard>
#include "Pages/PgDeviceList.hpp"
#include "Pages/PgNeedAuth.hpp"
#include "Pages/PgPairInit.hpp"
#include "Pages/PgPairConfirm.hpp"
#include "Pages/PgPairingInProgress.hpp"
#include "Pages/PgSucceeded.hpp"
#include "../Comm/UdpBroadcaster.hpp"





NewDeviceWizard::NewDeviceWizard(ComponentCollection & aComponents, QWidget * aParent):
	Super(aParent),
	mComponents(aComponents)
{
	setWindowTitle(tr("Deskemes: Add new device"));
	setWizardStyle(QWizard::ModernStyle);
	setPage(pgDeviceList,        new PgDeviceList       (mComponents, *this));
	setPage(pgNeedAuth,          new PgNeedAuth         (mComponents, *this));
	setPage(pgPairInit,          new PgPairInit         (mComponents, *this));
	setPage(pgPairConfirm,       new PgPairConfirm      (mComponents, *this));
	setPage(pgPairingInProgress, new PgPairingInProgress(mComponents, *this));
	setPage(pgSucceeded,         new PgSucceeded        (mComponents, *this));
	setStartId(pgDeviceList);

	// Start explicit discovery:
	mComponents.get<UdpBroadcaster>()->startDiscovery();
}





NewDeviceWizard::~NewDeviceWizard()
{
	mComponents.get<UdpBroadcaster>()->endDiscovery();
}





void NewDeviceWizard::setDevice(DetectedDevices::DevicePtr aDevice)
{
	mDevice = aDevice;
}





void NewDeviceWizard::setConnection(ConnectionPtr aConnection)
{
	mConnection = aConnection;
}
