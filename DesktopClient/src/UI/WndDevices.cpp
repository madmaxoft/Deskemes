#include "WndDevices.hpp"
#include <cassert>
#include <QLabel>
#include "ui_WndDevices.h"
#include "../Settings.hpp"
#include "../DeviceMgr.hpp"
#include "WgtDevice.hpp"
#include "NewDeviceWizard.hpp"
#include "DlgSendText.hpp"





WndDevices::WndDevices(ComponentCollection & aComponents, QWidget * aParent):
	Super(aParent),
	mComponents(aComponents),
	mUI(new Ui::WndDevices)
{
	mUI->setupUi(this);
	Settings::loadWindowPos("WndDevices", *this);

	auto mgr = mComponents.get<DeviceMgr>();
	connect(mgr.get(),              &DeviceMgr::deviceAdded,   this, &WndDevices::onDeviceAdded);
	connect(mgr.get(),              &DeviceMgr::deviceRemoved, this, &WndDevices::onDeviceRemoved);
	connect(mUI->actDeviceNew,      &QAction::triggered,       this, &WndDevices::addNewDevice);
	connect(mUI->actMessageSendNew, &QAction::triggered,       this, &WndDevices::sendNewMessage);
	connect(mUI->actExit,           &QAction::triggered,       this, &WndDevices::close);

	// Add devices already present:
	for (const auto & dev: mgr->devices())
	{
		addDeviceUI(dev);
	}
}





WndDevices::~WndDevices()
{
	Settings::saveWindowPos("WndDevices", *this);
}





void WndDevices::addDeviceUI(DevicePtr aDevice)
{
	assert(mDeviceWidgets.find(aDevice->deviceID()) == mDeviceWidgets.end());  // Not already present
	auto w = std::make_unique<WgtDevice>(aDevice);
	auto layout = qobject_cast<QBoxLayout *>(mUI->devicesContainer->layout());
	layout->insertWidget(layout->count() - 1, w.get(), 0, Qt::AlignLeft);
	mUI->lblNoDevices->hide();
	mDeviceWidgets[aDevice->deviceID()] = std::move(w);
}





void WndDevices::delDeviceUI(DevicePtr aDevice)
{
	auto itr = mDeviceWidgets.find(aDevice->deviceID());
	assert(itr != mDeviceWidgets.end());
	mDeviceWidgets.erase(itr);
}





void WndDevices::onDeviceAdded(DevicePtr aDevice)
{
	addDeviceUI(aDevice);
}





void WndDevices::onDeviceRemoved(DevicePtr aDevice)
{
	delDeviceUI(aDevice);
}





void WndDevices::addNewDevice()
{
	NewDeviceWizard wiz(mComponents, this);
	wiz.exec();
}





void WndDevices::sendNewMessage()
{
	DlgSendText dlg(mComponents, this);
	dlg.exec();
}
