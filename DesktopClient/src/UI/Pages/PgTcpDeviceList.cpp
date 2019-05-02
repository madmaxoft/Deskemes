#include "PgTcpDeviceList.hpp"
#include <cassert>
#include <QTableView>
#include "ui_PgTcpDeviceList.h"
#include "../../ComponentCollection.hpp"
#include "../../Comm/ConnectionMgr.hpp"
#include "../../Comm/UdpBroadcaster.hpp"
#include "../NewDeviceWizard.hpp"





PgTcpDeviceList::PgTcpDeviceList(ComponentCollection & aComponents, NewDeviceWizard & aParent):
	Super(&aParent),
	mComponents(aComponents),
	mParent(aParent),
	mUI(new Ui::PgTcpDeviceList)
{
	mUI->setupUi(this);
	connect(mUI->tvDevices, &QTableView::doubleClicked, this, &PgTcpDeviceList::deviceDoubleClicked);
}





PgTcpDeviceList::~PgTcpDeviceList()
{
	// Nothing explicit needed yet
}





void PgTcpDeviceList::initializePage()
{
	mDetectedDevices = mComponents.get<ConnectionMgr>()->detectDevices();
	mUI->tvDevices->setModel(mDetectedDevices.get());
	connect(mUI->tvDevices->selectionModel(), &QItemSelectionModel::selectionChanged, this, &QWizardPage::completeChanged);
	mComponents.get<UdpBroadcaster>()->startDiscovery();
}





bool PgTcpDeviceList::isComplete() const
{
	return (selectedDevice() != nullptr);
}





int PgTcpDeviceList::nextId() const
{
	auto sel = selectedDevice();
	if (sel == nullptr)
	{
		return NewDeviceWizard::pgFailed;
	}
	auto conn = mComponents.get<ConnectionMgr>()->connectionFromID(sel->enumeratorDeviceID());
	if (conn == nullptr)
	{
		return NewDeviceWizard::pgFailed;
	}
	mParent.setConnection(conn);
	switch (sel->status())
	{
		case DetectedDevices::Device::dsOnline:      return NewDeviceWizard::pgSucceeded;
		case DetectedDevices::Device::dsNoPubKey:    return NewDeviceWizard::pgPairInit;
		case DetectedDevices::Device::dsNeedPairing: return NewDeviceWizard::pgPairInit;
		case DetectedDevices::Device::dsBlacklisted: return NewDeviceWizard::pgBlacklisted;
		default:
		{
			assert(!"Unhandled device status");
			return -1;
		}
	}
}





DetectedDevices::DevicePtr PgTcpDeviceList::selectedDevice() const
{
	const auto & sel = mUI->tvDevices->selectionModel()->selectedRows();
	if (sel.size() == 0)
	{
		return nullptr;
	}
	return mDetectedDevices->data(sel[0], DetectedDevices::roleDevPtr).value<DetectedDevices::DevicePtr>();
}





void PgTcpDeviceList::deviceDoubleClicked(const QModelIndex & aIndex)
{
	mUI->tvDevices->selectRow(aIndex.row());
	if (isComplete())
	{
		wizard()->next();
	}
}
