#include "WgtDevice.hpp"
#include "ui_WgtDevice.h"





WgtDevice::WgtDevice(std::shared_ptr<Device> aDevice, QWidget * aParent):
	QWidget(aParent),
	mDevice(aDevice),
	mUI(new Ui::WgtDevice)
{
	mUI->setupUi(this);
}





WgtDevice::~WgtDevice()
{
	// Nothing explicit needed yet
}
