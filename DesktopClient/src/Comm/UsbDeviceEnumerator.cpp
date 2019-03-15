#include "UsbDeviceEnumerator.hpp"
#include "AdbCommunicator.hpp"
#include "DetectedDevices.hpp"





UsbDeviceEnumerator::UsbDeviceEnumerator(DetectedDevices & aDetectedDevices):
	mDetectedDevices(aDetectedDevices)
{
	setObjectName("UsbDeviceEnumerator");
	moveToThread(this);
	start();
}





UsbDeviceEnumerator::~UsbDeviceEnumerator()
{
	QThread::quit();
	if (!QThread::wait(1000))
	{
		qWarning() << "Failed to stop the UsbDeviceEnumerator thread, force-terminating.";
	}
}





void UsbDeviceEnumerator::run()
{
	auto adbDevList = std::make_unique<AdbCommunicator>(this);
	connect(adbDevList.get(), &AdbCommunicator::connected, adbDevList.get(), &AdbCommunicator::trackDevices);
	connect(adbDevList.get(), &AdbCommunicator::updateDeviceList, this, &UsbDeviceEnumerator::updateDeviceList);
	adbDevList->start();

	exec();

	adbDevList.reset();
}





void UsbDeviceEnumerator::requestDeviceScreenshot(const QByteArray & aDeviceID)
{
	QMetaObject::invokeMethod(
		this, "invRequestDeviceScreenshot",
		Qt::AutoConnection,
		Q_ARG(const QByteArray &, aDeviceID)
	);
}





void UsbDeviceEnumerator::invRequestDeviceScreenshot(const QByteArray & aDeviceID)
{
	auto adbScreenshotter = new AdbCommunicator(this);
	auto devID = aDeviceID;
	connect(adbScreenshotter, &AdbCommunicator::connected,          [=](){adbScreenshotter->assignDevice(devID);});
	connect(adbScreenshotter, &AdbCommunicator::deviceAssigned,     [=](){adbScreenshotter->takeScreenshot();});
	connect(adbScreenshotter, &AdbCommunicator::screenshotReceived, this, &UsbDeviceEnumerator::updateDeviceLastScreenshot);
	connect(adbScreenshotter, &AdbCommunicator::disconnected,       adbScreenshotter, &QObject::deleteLater);
	adbScreenshotter->start();
}





void UsbDeviceEnumerator::updateDeviceList(
	const QList<QByteArray> & aOnlineIDs,
	const QList<QByteArray> & aUnauthIDs,
	const QList<QByteArray> & aOtherIDs
)
{
	DetectedDevices::DeviceStatusList dsl;
	for (const auto & id: aOnlineIDs)
	{
		dsl.push_back({id, DetectedDevices::Device::dsOnline});
	}
	for (const auto & id: aUnauthIDs)
	{
		dsl.push_back({id, DetectedDevices::Device::dsUnauthorized});
	}
	for (const auto & id: aOtherIDs)
	{
		dsl.push_back({id, DetectedDevices::Device::dsOffline});
	}
	mDetectedDevices.updateDeviceList(dsl);
}





void UsbDeviceEnumerator::updateDeviceLastScreenshot(const QByteArray & aDeviceID, const QImage & aScreenshot)
{
	mDetectedDevices.setDeviceScreenshot(aDeviceID, aScreenshot);
}
