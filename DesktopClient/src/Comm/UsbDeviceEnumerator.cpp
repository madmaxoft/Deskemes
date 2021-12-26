#include "UsbDeviceEnumerator.hpp"
#include <QHostAddress>
#include <QNetworkInterface>
#include "AdbCommunicator.hpp"
#include "DetectedDevices.hpp"
#include "TcpListener.hpp"





UsbDeviceEnumerator::UsbDeviceEnumerator(ComponentCollection & aComponents):
	ComponentSuper(aComponents)
{
	requireForStart(ComponentCollection::ckTcpListener);
	setObjectName("UsbDeviceEnumerator");
	moveToThread(this);
	connect(&mDetectedDevices, &DetectedDevices::deviceAdded,         this, &UsbDeviceEnumerator::onDeviceAdded);
	connect(&mDetectedDevices, &DetectedDevices::deviceStatusChanged, this, &UsbDeviceEnumerator::onDeviceStatusChanged);
}





UsbDeviceEnumerator::~UsbDeviceEnumerator()
{
	QThread::quit();
	if (!QThread::wait(1000))
	{
		qWarning() << "Failed to stop the UsbDeviceEnumerator thread, force-terminating.";
	}
}





void UsbDeviceEnumerator::start()
{
	mTcpListenerPort = mComponents.get<TcpListener>()->listeningPort();
	QThread::start();
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
		Q_ARG(QByteArray, aDeviceID)
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





void UsbDeviceEnumerator::setupPortReversing(const QByteArray & aDeviceID)
{
	qDebug() << "Requesting port-reversing on device " << aDeviceID;
	auto adbReverser = new AdbCommunicator(this);
	auto devID = aDeviceID;
	connect(adbReverser, &AdbCommunicator::connected,      this, [=](){adbReverser->assignDevice(devID);});
	connect(adbReverser, &AdbCommunicator::deviceAssigned, this, [=](){adbReverser->portReverse(mTcpListenerPort, mTcpListenerPort);});
	connect(adbReverser, &AdbCommunicator::portReversingEstablished, this, &UsbDeviceEnumerator::startConnectionByIntent);
	connect(adbReverser, &AdbCommunicator::disconnected,   adbReverser, &QObject::deleteLater);
	adbReverser->start();
}





void UsbDeviceEnumerator::initiateConnectionViaAdb(const QByteArray & aDeviceID)
{
	qDebug() << "Initiating connection via ADB on device " << aDeviceID;
	auto adbStarter = new AdbCommunicator(this);
	auto devID = aDeviceID;
	auto shellCmd = QString::fromUtf8("am startservice -n cz.xoft.deskemes/.InitiateConnectionService --ei Port %1 --es Addresses \"").arg(mTcpListenerPort);

	// Append all our IPs that are not loopback:
	bool hasAddr = false;
	for (const auto & address: QNetworkInterface::allAddresses())
	{
		if (
			!address.isLoopback() &&
			(
				(address.protocol() == QAbstractSocket::IPv4Protocol) ||
				(address.protocol() == QAbstractSocket::IPv6Protocol)
			)
		)
		{
			auto addrCopy = address;
			addrCopy.setScopeId(QString());
			shellCmd += QString::fromUtf8(" %1").arg(addrCopy.toString());
			hasAddr = true;
		}
	}
	if (!hasAddr)
	{
		qDebug() << "No suitable local addresses found, cannot initiate connection.";
		adbStarter->deleteLater();
		return;
	}
	shellCmd += "\"";

	// Hand everything over to ADB:
	connect(adbStarter, &AdbCommunicator::connected,      [=](){adbStarter->assignDevice(devID);});
	connect(adbStarter, &AdbCommunicator::deviceAssigned, [=](){adbStarter->shellExecuteV1(shellCmd.toUtf8());});
	connect(adbStarter, &AdbCommunicator::disconnected,   adbStarter, &QObject::deleteLater);
	adbStarter->start();
}





void UsbDeviceEnumerator::startConnectionByIntent(const QByteArray & aDeviceID)
{
	qDebug() << "Attempting to start the connection from the app on device " << aDeviceID;
	auto appStarter = new AdbCommunicator(this);
	// auto shellCmd = QString::fromUtf8("am startservice -n cz.xoft.deskemes/.LocalConnectService --ei LocalPort %1 >/dev/null").arg(mTcpListenerPort);
	auto shellCmd = QString::fromUtf8("am startservice -n cz.xoft.deskemes/.LocalConnectService --ei LocalPort %1").arg(mTcpListenerPort);
	auto devID = aDeviceID;
	std::string shellOutput;
	connect(appStarter, &AdbCommunicator::connected,      [=](){appStarter->assignDevice(devID);});
	connect(appStarter, &AdbCommunicator::deviceAssigned, [=](){appStarter->shellExecuteV1(shellCmd.toUtf8());});
	connect(appStarter, &AdbCommunicator::shellIncomingData,
		[](const QByteArray & aDeviceID, const QByteArray & aShellOutOrErr)
		{
			if (!aShellOutOrErr.isEmpty())
			{
				qDebug() << "Connection intent failed to start on device " << aDeviceID << ", message: " << aShellOutOrErr;
			}
		}
	);
	connect(appStarter, &AdbCommunicator::error, 
		[](const QString & aErrorText)
		{
			qDebug() << "Error while starting connection by intent: " << aErrorText;
		}
	);
	connect(appStarter, &AdbCommunicator::disconnected, appStarter, &QObject::deleteLater);
	appStarter->start();
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
	mDetectedDevices.setDeviceAvatar(aDeviceID, aScreenshot);
}





void UsbDeviceEnumerator::onDeviceAdded(const QByteArray & aDeviceID, DetectedDevices::Device::Status aStatus)
{
	if (aStatus == DetectedDevices::Device::dsOnline)
	{
		// Doesn't work on some devices:
		// setupPortReversing(aDeviceID);
		initiateConnectionViaAdb(aDeviceID);
	}
}





void UsbDeviceEnumerator::onDeviceStatusChanged(const QByteArray & aDeviceID, DetectedDevices::Device::Status aStatus)
{
	if (aStatus == DetectedDevices::Device::dsOnline)
	{
		// Doesn't work on some devices:
		// setupPortReversing(aDeviceID);
		initiateConnectionViaAdb(aDeviceID);
	}
}
