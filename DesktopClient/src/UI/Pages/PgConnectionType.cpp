#include "PgConnectionType.hpp"
#include <cassert>
#include "ui_PgConnectionType.h"
#include "../NewDeviceWizard.hpp"





PgConnectionType::PgConnectionType(QWidget * aParent):
	Super(aParent),
	mUI(new Ui::PgConnectionType)
{
	mUI->setupUi(this);
	setTitle(tr("Connection type:"));
	setSubTitle(tr("Please select the type of connection you would like to use:"));
}





PgConnectionType::~PgConnectionType()
{
	// Nothing explicit needed
}





int PgConnectionType::nextId() const
{
	if (mUI->rbConnTcp->isChecked())
	{
		return NewDeviceWizard::pgTcpDeviceList;
	}
	if (mUI->rbConnBluetooth->isChecked())
	{
		return NewDeviceWizard::pgBluetoothDeviceList;
	}
	if (mUI->rbConnUsb->isChecked())
	{
		return NewDeviceWizard::pgUsbDeviceList;
	}
	assert(!"Unknown connection type");
	return -1;
}
