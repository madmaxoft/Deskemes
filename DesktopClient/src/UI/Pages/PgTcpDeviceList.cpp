#include "PgTcpDeviceList.hpp"
#include "ui_PgTcpDeviceList.h"





PgTcpDeviceList::PgTcpDeviceList(QWidget * aParent):
	Super(aParent),
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
	// TODO: Start the TCP detection
}
