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
	auto usbEnumerator = mComponents.get<UsbDeviceEnumerator>();
	connect(&usbEnumerator->detectedDevices(), &DetectedDevices::deviceAdded,
		[usbEnumerator](const QByteArray & aDeviceID, DetectedDevices::Device::Status aStatus)
		{
			if (aStatus == DetectedDevices::Device::dsOnline)
			{
				usbEnumerator->requestDeviceScreenshot(aDeviceID);
			}
		}
	);
}
