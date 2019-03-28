#include "PgTcpDeviceList.hpp"
#include "ui_PgTcpDeviceList.h"
#include "../../ComponentCollection.hpp"
#include "../../Comm/TcpListener.hpp"





PgTcpDeviceList::PgTcpDeviceList(ComponentCollection & aComponents, QWidget * aParent):
	Super(aParent),
	mComponents(aComponents),
	mUI(new Ui::PgTcpDeviceList)
{
	mUI->setupUi(this);
}





PgTcpDeviceList::~PgTcpDeviceList()
{
	// Nothing explicit needed yet
}





void PgTcpDeviceList::initializePage()
{
	mDetectedDevices = mComponents.get<TcpListener>()->detectDevices();
	mUI->tvDevices->setModel(mDetectedDevices.get());
}
