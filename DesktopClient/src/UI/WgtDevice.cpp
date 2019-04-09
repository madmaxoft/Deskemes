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
}





WgtDevice::~WgtDevice()
{
	// Nothing explicit needed yet
}
