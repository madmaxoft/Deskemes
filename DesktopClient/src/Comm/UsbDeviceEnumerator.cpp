#include "UsbDeviceEnumerator.hpp"
#include <QHostAddress>
#include <QNetworkInterface>
#include <QProcess>
#include "AdbCommunicator.hpp"
#include "DetectedDevices.hpp"
#include "TcpListener.hpp"





UsbDeviceEnumerator::UsbDeviceEnumerator(ComponentCollection & aComponents):
	ComponentSuper(aComponents),
	mIsAdbAvailable(false),
	mLogger(aComponents.logger("UsbDeviceEnumerator"))
{
	requireForStart(ComponentCollection::ckTcpListener);
	requireForStart(ComponentCollection::ckDetectedDevices);

	setObjectName("UsbDeviceEnumerator");
	moveToThread(this);
}





UsbDeviceEnumerator::~UsbDeviceEnumerator()
{
	mLogger.log("Terminating...");
	QThread::quit();
	if (!QThread::wait(1000))
	{
		mLogger.log("ERROR: Failed to stop the UsbDeviceEnumerator thread, force-terminating.");
	}
}





void UsbDeviceEnumerator::start()
{
	mTcpListenerPort = mComponents.get<TcpListener>()->listeningPort();
	QThread::start();
}





void UsbDeviceEnumerator::run()
{
	// Try to start the ADB server, if not running already:
	if (QProcess::execute("adb", {"devices"}) != 0)
	{
		mIsAdbAvailable = false;
	}

	// Set up the device tracking:
	auto & devListLogger = mComponents.logger("UsbDeviceEnumerator-DevList");
	auto adbDevList = std::make_unique<AdbCommunicator>(devListLogger);
	adbDevList->moveToThread(this);
	connect(adbDevList.get(), &AdbCommunicator::connected,        adbDevList.get(), &AdbCommunicator::trackDevices);
	connect(adbDevList.get(), &AdbCommunicator::updateDeviceList, this,             &UsbDeviceEnumerator::updateDeviceList);
	adbDevList->start();

	// Also set up periodic device queries:
	QTimer timer;
	connect(&timer, &QTimer::timeout, this, [=, &devListLogger]()
		{
			auto adbDevLister = new AdbCommunicator(devListLogger);
			adbDevLister->moveToThread(this);
			connect(adbDevLister, &AdbCommunicator::updateDeviceList, this,         &UsbDeviceEnumerator::updateDeviceList);
			connect(adbDevLister, &AdbCommunicator::connected,        adbDevLister, [=](){adbDevLister->listDevices();});
			connect(adbDevLister, &AdbCommunicator::disconnected,     adbDevLister, &QObject::deleteLater);
			adbDevLister->start();
		}
	);
	timer.start(1000);

	exec();

	timer.stop();
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
	mLogger.log("Requesting screenshot from device %1", aDeviceID);
	auto adbScreenshotter = new AdbCommunicator(loggerForDevice(aDeviceID));
	adbScreenshotter->moveToThread(this);
	auto devID = aDeviceID;
	connect(adbScreenshotter, &AdbCommunicator::connected,          adbScreenshotter, [=](){adbScreenshotter->assignDevice(devID);});
	connect(adbScreenshotter, &AdbCommunicator::deviceAssigned,     adbScreenshotter, [=](){adbScreenshotter->takeScreenshot();});
	connect(adbScreenshotter, &AdbCommunicator::screenshotReceived, this,             &UsbDeviceEnumerator::updateDeviceLastScreenshot);
	connect(adbScreenshotter, &AdbCommunicator::disconnected,       adbScreenshotter, &QObject::deleteLater);
	connect(adbScreenshotter, &AdbCommunicator::error, this,
		[=](const QString & aErrorText)
		{
			mLogger.log("Device %1  has failed to produce screenshot: %2.", devID, aErrorText);
			mDevicesFailedScreenshot.insert(devID);
		}
	);
	adbScreenshotter->start();
}





void UsbDeviceEnumerator::tryStartApp(const QByteArray & aDeviceID)
{
	// If detection is already running on the device, bail out:
	auto itr = mDetectionStatus.find(aDeviceID);
	if (itr != mDetectionStatus.cend())
	{
		return;
	}

	mLogger.log("Attempting to start the app on device %1.", aDeviceID);
	mDetectionStatus[aDeviceID];

	auto shellCmd = QString::fromUtf8("pm list packages cz.xoft.deskemes");
	auto devID = aDeviceID;
	auto appQuery = new AdbCommunicator(loggerForDevice(aDeviceID));
	appQuery->moveToThread(this);
	connect(appQuery, &AdbCommunicator::connected,         this, [=](){appQuery->assignDevice(devID);});
	connect(appQuery, &AdbCommunicator::deviceAssigned,    this, [=](){appQuery->shellExecuteV1(shellCmd.toUtf8());});
	connect(appQuery, &AdbCommunicator::shellIncomingData, this,
		[this](const QByteArray & aDeviceID, const QByteArray & aShellOutOrErr)
		{
			mDetectionStatus[aDeviceID].mAppPackageQueryOutput.append(aShellOutOrErr);
		}
	);
	connect(appQuery, &AdbCommunicator::error, this,
		[this, devID](const QString & aErrorText)
		{
			mLogger.log("Error while checking on %1 for app: %2.", devID, aErrorText);
			auto itr = mDetectionStatus.find(devID);
			if (itr != mDetectionStatus.end())
			{
				mDetectionStatus.erase(itr);
			}
		}
	);
	connect(appQuery, &AdbCommunicator::disconnected, this,
		[this, devID, appQuery]()
		{
			appQuery->deleteLater();
			auto itr = mDetectionStatus.find(devID);
			bool hasApp = false;
			if (itr != mDetectionStatus.end())
			{
				hasApp = itr->second.mAppPackageQueryOutput.contains("cz.xoft.deskemes");
			}
			if (hasApp)
			{
				setupPortReversingAndConnect(devID);
			}
			else
			{
				mLogger.log("Device %1 doesn't have the app installed.", devID);
				auto dd = mComponents.get<DetectedDevices>();
				dd->setDeviceStatus(mKind, devID, DetectedDevices::Device::dsNeedApp);
				mDetectionStatus.erase(itr);
			}
		}
	);

	appQuery->start();
}





void UsbDeviceEnumerator::setupPortReversingAndConnect(const QByteArray & aDeviceID)
{
	mLogger.log("Requesting port-reversing on device %1.", aDeviceID);
	auto adbReverser = new AdbCommunicator(loggerForDevice(aDeviceID));
	adbReverser->moveToThread(this);
	auto devID = aDeviceID;
	connect(adbReverser, &AdbCommunicator::connected,                this,        [=](){adbReverser->assignDevice(devID);});
	connect(adbReverser, &AdbCommunicator::deviceAssigned,           this,        [=](){adbReverser->portReverse(mTcpListenerPort, mTcpListenerPort);});
	connect(adbReverser, &AdbCommunicator::portReversingEstablished, this,        &UsbDeviceEnumerator::startConnectionToApp);
	connect(adbReverser, &AdbCommunicator::disconnected,             adbReverser, &QObject::deleteLater);
	connect(adbReverser, &AdbCommunicator::error,                    this,
		[=](QString aErrorText)
		{
			mLogger.log("Port reversing setup on device %1 failed: %2", devID, aErrorText);
			mComponents.get<DetectedDevices>()->setDeviceStatus(mKind, devID, DetectedDevices::Device::dsFailed);
		}
	);
	adbReverser->start();
}





void UsbDeviceEnumerator::startConnectionToApp(const QByteArray & aDeviceID)
{
	mLogger.log("Attempting to start the connection from the app on device %1.", aDeviceID);

	// Build the shell commandline with all our IPs that are not loopback:
	auto shellCmd = QString::fromUtf8("am startservice -n cz.xoft.deskemes/.InitiateConnectionService --ei LocalPort %1 --ei Port %1 --es Addresses \"").arg(mTcpListenerPort);
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
		}
	}
	shellCmd += "\"";

	auto devID = aDeviceID;
	auto appStarter = new AdbCommunicator(loggerForDevice(devID));
	appStarter->moveToThread(this);
	connect(appStarter, &AdbCommunicator::connected,      this, [=](){appStarter->assignDevice(devID);});
	connect(appStarter, &AdbCommunicator::deviceAssigned, this, [=](){appStarter->shellExecuteV1(shellCmd.toUtf8());});
	connect(appStarter, &AdbCommunicator::disconnected,   appStarter, &QObject::deleteLater);
	connect(appStarter, &AdbCommunicator::shellIncomingData, this,
		[=](const QByteArray & aDeviceID, const QByteArray & aShellOutOrErr)
		{
			mDetectionStatus[aDeviceID].mStartServiceOutput.append(aShellOutOrErr);
		}
	);
	connect(appStarter, &AdbCommunicator::disconnected, this,
		[=]()
		{
			appStarter->deleteLater();
			auto shellOutput = std::move(mDetectionStatus[devID].mStartServiceOutput);
			mDetectionStatus.erase(aDeviceID);
			if (shellOutput.isEmpty())
			{
				// There was no error message, assume the service was started
				// The device should connect via regular TCP now
				// TODO: What if the device still needs pairing?
				mComponents.get<DetectedDevices>()->setDeviceStatus(mKind, devID, DetectedDevices::Device::dsOnline);
			}
			else
			{
				mComponents.get<DetectedDevices>()->setDeviceStatus(mKind, devID, DetectedDevices::Device::dsFailed);
			}
		}
	);
	connect(appStarter, &AdbCommunicator::error, this,
		[=](const QString & aErrorText)
		{
			mLogger.log("Error while starting connection to %1 by intent: %2", devID, aErrorText);
			mComponents.get<DetectedDevices>()->setDeviceStatus(mKind, devID, DetectedDevices::Device::dsFailed);
		}
	);
	appStarter->start();
}





Logger & UsbDeviceEnumerator::loggerForDevice(const QString & aDeviceID)
{
	return mComponents.logger("UsbDeviceEnumerator-" + aDeviceID);
}





void UsbDeviceEnumerator::updateDeviceList(
	const QList<QByteArray> & aOnlineIDs,
	const QList<QByteArray> & aUnauthIDs,
	const QList<QByteArray> & aOtherIDs
)
{
	auto dd = mComponents.get<DetectedDevices>();
	auto devEntries = dd->allEnumeratorDevices(mKind);

	// Set devices no longer tracked as offline:
	for (const auto & devEntry: devEntries)
	{
		const auto & id = devEntry.second->enumeratorDeviceID();
		if (
			!aOnlineIDs.contains(id) &&
			!aUnauthIDs.contains(id) &&
			!aOtherIDs.contains(id)
		)
		{
			dd->setDeviceStatus(mKind, id, DetectedDevices::Device::dsOffline);
		}
	}

	// Set all unauth devices as dsUnauthorized:
	for (const auto & id: aUnauthIDs)
	{
		dd->setDeviceStatus(mKind, id, DetectedDevices::Device::dsUnauthorized);
	}

	// Set all other devices as dsOffline; forget about screenshot failures:
	for (const auto & id: aOtherIDs)
	{
		dd->setDeviceStatus(mKind, id, DetectedDevices::Device::dsOffline);
		mDevicesFailedScreenshot.erase(id);
	}

	// Request a screenshot for all online devices that haven't failed to screenshot before:
	for (const auto & id: aOnlineIDs)
	{
		if (mDevicesFailedScreenshot.find(id) != mDevicesFailedScreenshot.cend())
		{
			continue;
		}
		auto devItr = devEntries.find(id);
		if (devItr != devEntries.cend())
		{
			if (devItr->second->avatar().isNull())
			{
				requestDeviceScreenshot(id);
			}
		}
	}

	// New online devs need to check the app presence to determine their status:
	for (const auto & id: aOnlineIDs)
	{
		auto devItr = devEntries.find(id);
		if (devItr == devEntries.cend())
		{
			// The device is new, try connecting to its app:
			tryStartApp(id);
			continue;
		}
		// The device has been known before, check if the status could use updating:
		if (devItr->second->status() == DetectedDevices::Device::dsNeedApp)
		{
			// We didn't detect the app before, perhaps it has been installed in the meantime?
			tryStartApp(id);
			continue;
		}
	}
}





void UsbDeviceEnumerator::updateDeviceLastScreenshot(const QByteArray & aDeviceID, const QImage & aScreenshot)
{
	mComponents.get<DetectedDevices>()->setDeviceAvatar(aDeviceID, aScreenshot);
}
