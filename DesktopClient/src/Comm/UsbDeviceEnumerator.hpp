#pragma once

#include <atomic>
#include <map>
#include <set>

#include <QThread>
#include <QTimer>
#include <QImage>
#include <QMutex>
#include "../ComponentCollection.hpp"
#include "DetectedDevices.hpp"





// fwd:
class AdbCommunicator;





/** An enumerator that uses ADB over USB to query and connect devices.
If ADB is not available, it doesn't report any devices.
The devices are reported into a DetectedDevices instance that is used to thread-sync the changes and
provide a UI model for the devices.
Basically a thread for the enumerating and container for the various ADB connections; the heavy lifting
is done in AdbCommunicator. */
class UsbDeviceEnumerator:
	public QThread,
	public ComponentCollection::Component<ComponentCollection::ckUsbDeviceEnumerator>
{
	using Super = QThread;
	using ComponentSuper = ComponentCollection::Component<ComponentCollection::ckUsbDeviceEnumerator>;

	Q_OBJECT


public:


	/** Creates a new enumerator and makes it store info in the supplied DetectedDevices instance. */
	UsbDeviceEnumerator(ComponentCollection & aComponents);

	virtual ~UsbDeviceEnumerator() override;

	/** Starts the enumerator. */
	virtual void start() override;

	/** Indicates whether ADB executable can be started.
	Initialized upon thread start, never updated (need to restart app to re-detect). */
	bool isAdbAvailable() const { return mIsAdbAvailable; }


protected:

	/** Temporary data needed while detection is in progress on a device. */
	struct DetectionStatus
	{
		/** Shell output of the package query ("pm list packages cz.xoft.deskemes").
		Used to detect whether the app is present, in tryStartApp(). */
		QByteArray mAppPackageQueryOutput;

		/** Shell output of the service start command ("am startservice ...").
		Used to detect whether the service was started successfully. */
		QByteArray mStartServiceOutput;
	};


	/** The TCP port to which the traffic from the devices is redirected using ADB port-forwaring. */
	quint16 mTcpListenerPort;

	/** The temporary data needed for each device while detection is in progress on it.
	Map of DeviceID -> Status.
	To be accessed only from this object's thread. */
	std::map<QByteArray, DetectionStatus> mDetectionStatus;

	/** Tracks the devices for which the screenshot command has failed (so that no further screenshots will be requested for them).
	Map of DeviceID -> bool (true = failed).
	To be accessed only from this object's thread. */
	std::set<QByteArray> mDevicesFailedScreenshot;

	/** Indicates whether ADB executable can be started.
	Initialized upon thread start, never updated (need to restart app to re-detect). */
	bool mIsAdbAvailable;

	/** The logger used for all messages produced by this class. */
	Logger & mLogger;


	// QThread overrides:
	virtual void run() override;

	/** Requests a new screenshot from the specified device.
	Guaranteed to be called from this object's thread. */
	Q_INVOKABLE void invRequestDeviceScreenshot(const QByteArray & aDeviceID);

	/** Asynchronously tries to start the app and connect to it.
	First checks if the app is installed ("pm list packages"); if not, sets the device as dsNeedApp and finishes.
	Then it sets up the port reversing through ADB.
	Then it tries to start the app and make it connect both via USB and TCP. If all succeeds, sets the device as dsOnline. */
	void tryStartApp(const QByteArray & aDeviceID);

	/** Tries to set up port-reversing on the device, then calls startConnectionByIntent(). */
	void setupPortReversingAndConnect(const QByteArray & aDeviceID);

	/** Tries to start up the on-device app and make it connect both through USB and TCP.
	USB connection is attempted by invoking the LocalConnectService using an intent.
	TCP connection is attempted by invoking the InitiateConnectionService intent for all our IPs.
	This will "ping" the app to connect to the local port, previously port-reversed through ADB to local computer.
	Updates the device state in DetectedDeviecs based on the result (dsNeedApp / dsOnline) */
	void startConnectionToApp(const QByteArray & aDeviceID);

	/** Returns the logger to use for a specific device's AdbCommunicator. */
	Logger & loggerForDevice(const QString & aDeviceID);


public Q_SLOTS:

	/** Requests a new screenshot from the specified device.
	Relays the request onto this object's thread by invoking invRequestDeviceScreenshot(). */
	void requestDeviceScreenshot(const QByteArray & aDevice);


protected Q_SLOTS:

	/** Updates the internal list of devices based on the given IDs.
	Called from the run::adbDevList when the device list changes. */
	void updateDeviceList(
		const QList<QByteArray> & aOnlineIDs,
		const QList<QByteArray> & aUnauthIDs,
		const QList<QByteArray> & aOtherIDs
	);

	/** Updates the last screenshot stored in mDevices[] for the specified device.
	If the device is not in mDevices[], ignored. */
	void updateDeviceLastScreenshot(const QByteArray & aDeviceID, const QImage & aScreenshot);
};
