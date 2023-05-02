#include "WgtDevice.hpp"
#include "ui_WgtDevice.h"
#include "../Device.hpp"





WgtDevice::WgtDevice(std::shared_ptr<Device> aDevice, QWidget * aParent):
	QWidget(aParent),
	mUI(new Ui::WgtDevice),
	mDevice(aDevice)
{
	mUI->setupUi(this);
	mUI->lblFriendlyName->setText(aDevice->friendlyName());

	// Connect the device signals:
	connect(aDevice.get(), &Device::identificationUpdated, this, &WgtDevice::updateIdentification);
	connect(aDevice.get(), &Device::batteryUpdated,        this, &WgtDevice::updateBattery);
	connect(aDevice.get(), &Device::signalStrengthUpdated, this, &WgtDevice::updateSignalStrength);
}





WgtDevice::~WgtDevice()
{
	// Nothing explicit needed yet
}





void WgtDevice::updateIdentification(const QString & aImei, const QString & aImsi, const QString & aCarrierName)
{
	mUI->leImei->setText(aImei);
	mUI->leImsi->setText(aImsi);
	mUI->leCarrierName->setText(aCarrierName);
}





void WgtDevice::updateBattery(const double aBatteryLevelPercent)
{
	mUI->pbBattery->setValue(static_cast<int>(aBatteryLevelPercent));
}





void WgtDevice::updateSignalStrength(const double aSignalStrengthPercent)
{
	mUI->pbSignalStrength->setValue(static_cast<int>(aSignalStrengthPercent));
}
