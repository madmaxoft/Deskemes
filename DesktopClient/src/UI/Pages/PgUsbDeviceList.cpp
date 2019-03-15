#include "PgUsbDeviceList.hpp"
#include "ui_PgUsbDeviceList.h"
#include "../../Comm/UsbDeviceEnumerator.hpp"





PgUsbDeviceList::PgUsbDeviceList(ComponentCollection & aComponents, QWidget * aParent):
	Super(aParent),
	mComponents(aComponents),
	mUI(new Ui::PgUsbDeviceList)
{
	mUI->setupUi(this);
	mUI->tvDevices->setModel(&mDetectedDevices);
}





PgUsbDeviceList::~PgUsbDeviceList()
{
	// Nothing explicit needed yet
}





void PgUsbDeviceList::initializePage()
{
	mDevEnum = std::make_unique<UsbDeviceEnumerator>(mDetectedDevices);
	connect(&mDetectedDevices, &DetectedDevices::deviceAdded,
		[this](const QByteArray & aDeviceID, DetectedDevices::Device::Status aStatus)
		{
			if (aStatus == DetectedDevices::Device::dsOnline)
			{
				mDevEnum->requestDeviceScreenshot(aDeviceID);
			}
		}
	);
	// mDevEnum.get(), &UsbDeviceEnumerator::requestDeviceScreenshot);
}
