#include "NewDeviceWizard.hpp"
#include <QWizard>
#include "Pages/PgConnectionType.hpp"
#include "Pages/PgTcpDeviceList.hpp"
#include "Pages/PgUsbDeviceList.hpp"
#include "Pages/PgPairInit.hpp"
#include "Pages/PgPairConfirm.hpp"
#include "Pages/PgPairingInProgress.hpp"
#include "Pages/PgSucceeded.hpp"





NewDeviceWizard::NewDeviceWizard(ComponentCollection & aComponents, QWidget * aParent):
	Super(aParent),
	mComponents(aComponents)
{
	setWindowTitle(tr("Deskemes: Add new device"));
	setWizardStyle(QWizard::ModernStyle);
	setPage(pgConnectionType,    new PgConnectionType(this));
	setPage(pgTcpDeviceList,     new PgTcpDeviceList    (mComponents, *this));
	setPage(pgUsbDeviceList,     new PgUsbDeviceList    (mComponents, this));
	setPage(pgPairInit,          new PgPairInit         (mComponents, *this));
	setPage(pgPairConfirm,       new PgPairConfirm      (mComponents, *this));
	setPage(pgPairingInProgress, new PgPairingInProgress(mComponents, *this));
	setPage(pgSucceeded,         new PgSucceeded        (mComponents, *this));
	setStartId(pgConnectionType);
}





void NewDeviceWizard::setConnection(ConnectionPtr aConnection)
{
	mConnection = aConnection;
}
