#include "WndDevices.hpp"
#include <cassert>
#include <QLabel>
#include "ui_WndDevices.h"
#include "../Settings.hpp"
#include "../Devices.hpp"
#include "WgtDevice.hpp"
#include "NewDeviceWizard.hpp"





WndDevices::WndDevices(ComponentCollection & aComponents, QWidget * aParent):
	Super(aParent),
	mComponents(aComponents),
	mUI(new Ui::WndDevices)
{
	mUI->setupUi(this);
	Settings::loadWindowPos("WndDevices", *this);

	auto devs = mComponents.get<Devices>();
	connect(devs.get(), &Devices::deviceAdded,   this, &WndDevices::onDeviceAdded);
	connect(devs.get(), &Devices::deviceRemoved, this, &WndDevices::onDeviceRemoved);
	connect(mUI->actDeviceNew, &QAction::triggered, this, &WndDevices::addNewDevice);

	// Add devices already present:
	for (const auto & dev: devs->devices())
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
