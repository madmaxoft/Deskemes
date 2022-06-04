#include "PgDeviceList.hpp"
#include <cassert>
#include <QTableView>
#include "ui_PgDeviceList.h"
#include "../../ComponentCollection.hpp"
#include "../../Comm/ConnectionMgr.hpp"
#include "../NewDeviceWizard.hpp"
#include "../DetectedDevicesModel.hpp"





PgDeviceList::PgDeviceList(ComponentCollection & aComponents, NewDeviceWizard & aParent):
	Super(&aParent),
	mComponents(aComponents),
	mParent(aParent),
	mUI(new Ui::PgDeviceList)
{
	mUI->setupUi(this);
	connect(mUI->tvDevices, &QTableView::doubleClicked, this, &PgDeviceList::deviceDoubleClicked);
}





PgDeviceList::~PgDeviceList()
{
	// Nothing explicit needed yet
}





void PgDeviceList::initializePage()
{
	mDetectedDevicesModel.reset(new DetectedDevicesModel(mComponents));
	mUI->tvDevices->setModel(mDetectedDevicesModel.get());
	connect(mUI->tvDevices->selectionModel(), &QItemSelectionModel::selectionChanged, this, &QWizardPage::completeChanged);
}





bool PgDeviceList::isComplete() const
{
	return (selectedDevice() != nullptr);
}





int PgDeviceList::nextId() const
{
	auto sel = selectedDevice();
	if (sel == nullptr)
	{
		mParent.logger().log("PgDeviceList::nextId(): No selected device, reporting failure.");
		return NewDeviceWizard::pgFailed;
	}

	// If the device is already connected, exit the wizard:
	auto conn = mComponents.get<ConnectionMgr>()->connectionFromID(sel->enumeratorDeviceID());
	if (conn != nullptr)
	{
		mParent.logger().log("PgDeviceList::nextId(): The device is already connected through %1, finishing the wizard", conn->connectionID());
		mParent.setConnection(conn);
		return NewDeviceWizard::pgSucceeded;
	}

	// Continue to the page relevant to the device's status:
	mParent.setDevice(sel);
	switch (sel->status())
	{
		case DetectedDevices::Device::dsOnline:       return NewDeviceWizard::pgSucceeded;
		case DetectedDevices::Device::dsUnauthorized: return NewDeviceWizard::pgNeedAuth;
		case DetectedDevices::Device::dsNoPubKey:     return NewDeviceWizard::pgPairInit;
		case DetectedDevices::Device::dsNeedPairing:  return NewDeviceWizard::pgPairInit;
		case DetectedDevices::Device::dsBlacklisted:  return NewDeviceWizard::pgBlacklisted;
		case DetectedDevices::Device::dsOffline:      return NewDeviceWizard::pgFailed;
		case DetectedDevices::Device::dsNeedApp:      return NewDeviceWizard::pgNeedApp;
		case DetectedDevices::Device::dsFailed:       return NewDeviceWizard::pgFailed;
	}
	#ifdef _MSC_VER
		assert(!"Unknown device status");
		throw LogicError("Unknown device status: %1", sel->status());
	#endif
}





DetectedDevices::DevicePtr PgDeviceList::selectedDevice() const
{
	const auto & sel = mUI->tvDevices->selectionModel()->selectedRows();
	if (sel.size() == 0)
	{
		return nullptr;
	}
	return mDetectedDevicesModel->data(sel[0], DetectedDevicesModel::roleDevPtr).value<DetectedDevices::DevicePtr>();
}





void PgDeviceList::deviceDoubleClicked(const QModelIndex & aIndex)
{
	mParent.logger().log("deviceDoubleClicked: %1", aIndex.row());
	mUI->tvDevices->selectRow(aIndex.row());
	if (isComplete())
	{
		mParent.logger().log("DblClick: Advancing the wizard");
		wizard()->next();
	}
}
