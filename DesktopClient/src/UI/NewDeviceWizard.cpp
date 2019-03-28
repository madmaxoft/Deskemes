#include "NewDeviceWizard.hpp"
#include <QWizard>
#include "Pages/PgConnectionType.hpp"
#include "Pages/PgTcpDeviceList.hpp"
#include "Pages/PgUsbDeviceList.hpp"





NewDeviceWizard::NewDeviceWizard(ComponentCollection & aComponents, QWidget * aParent):
	Super(aParent),
	mComponents(aComponents)
{
	setWindowTitle(tr("Deskemes: Add new device"));
	setWizardStyle(QWizard::ModernStyle);
	setPage(pgConnectionType, new PgConnectionType(this));
	setPage(pgTcpDeviceList,  new PgTcpDeviceList(mComponents, this));
	setPage(pgUsbDeviceList,  new PgUsbDeviceList(mComponents, this));
	setStartId(pgConnectionType);
}
