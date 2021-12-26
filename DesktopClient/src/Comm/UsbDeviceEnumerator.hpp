#pragma once

#include <atomic>
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


protected:

	/** The TCP port to which the traffic from the devices is redirected using ADB port-forwaring. */
	quint16 mTcpListenerPort;


	// QThread overrides:
	virtual void run() override;

	/** Requests a new screenshot from the specified device.
	Guaranteed to be called from this object's thread. */
	Q_INVOKABLE void invRequestDeviceScreenshot(const QByteArray & aDeviceID);

	/** Tries to set up port-reversing on the device, then calls startConnectionByIntent().
	Called from mDetectedDevices when a new device is added to the list. */
	void setupPortReversing(const QByteArray & aDeviceID);

	/** Tries to make app initiate a connection to us by sending a InitiateConnectionServiec intent for all our IPs.
	Called from mDetectedDevices when a new device is added to the list or becomes online. */
	void initiateConnectionViaAdb(const QByteArray & aDeviceID);

	/** Tries to start up the on-device app by invoking the LocalConnectService using an intent.
	This will "ping" the app to connect to the local port, previously port-reversed through ADB to local computer. */
	void startConnectionByIntent(const QByteArray & aDeviceID);


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
