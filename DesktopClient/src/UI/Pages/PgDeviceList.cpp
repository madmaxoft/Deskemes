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
	mUI->tvDevices->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
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
		mParent.logger().log("PgDeviceList::nextId(): The device is already connected through %1 in state %2 (%3).",
			conn->connectionID(),
			conn->state(),
			Connection::stateToString(conn->state())
		);
		mParent.setConnection(conn);
		switch (conn->state())
		{
			case Connection::State::csInitial:
			{
				// This connection doesn't help at all, continue with the regular wizard:
				break;
			}
			case Connection::State::csUnknownPairing:    return NewDeviceWizard::pgPairInit;
			case Connection::State::csKnownPairing:      return NewDeviceWizard::pgSucceeded;  // TODO: This may be misleading, we need an "in progress" page
			case Connection::State::csRequestedPairing:  return NewDeviceWizard::pgPairInit;
			case Connection::State::csBlacklisted:       return NewDeviceWizard::pgBlacklisted;
			case Connection::State::csDifferentKey:      return NewDeviceWizard::pgBlacklisted;  // TODO: Should we report this more explicitly?
			case Connection::State::csEncrypted:         return NewDeviceWizard::pgSucceeded;
			case Connection::State::csDisconnected:      return NewDeviceWizard::pgFailed;
		}
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
