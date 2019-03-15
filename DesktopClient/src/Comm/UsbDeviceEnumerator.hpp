#pragma once

#include <atomic>
#include <QThread>
#include <QTimer>
#include <QImage>
#include <QMutex>





// fwd:
class AdbCommunicator;
class DetectedDevices;





/** An enumerator that uses ADB over USB to query and connect devices.
If ADB is not available, it doesn't report any devices.
The devices are reported into a DetectedDevices instance that is used to thread-sync the changes and
provide a UI model for the devices.
Basically a thread for the enumerating and container for the various ADB connections; the heavy lifting
is done in AdbCommunicator. */
class UsbDeviceEnumerator:
	public QThread
{
	Q_OBJECT

public:


	/** Creates a new enumerator and makes it store info in the supplied DetectedDevices instance. */
	UsbDeviceEnumerator(DetectedDevices & aDetectedDevices);

	virtual ~UsbDeviceEnumerator() override;


protected:

	/** The storage for the detected devices. */
	DetectedDevices & mDetectedDevices;

	// QThread overrides:
	virtual void run() override;

	/** Requests a new screenshot from the specified device.
	Guaranteed to be called from this object's thread. */
	Q_INVOKABLE void invRequestDeviceScreenshot(const QByteArray & aDeviceID);


public slots:

	/** Requests a new screenshot from the specified device.
	Relays the request onto this object's thread by invoking invRequestDeviceScreenshot(). */
	void requestDeviceScreenshot(const QByteArray & aDevice);


protected slots:

	/** Updates the internal list of devices based on the given IDs.
	Called from the mAdbDevList when the device list changes. */
	void updateDeviceList(
		const QList<QByteArray> & aOnlineIDs,
		const QList<QByteArray> & aUnauthIDs,
		const QList<QByteArray> & aOtherIDs
	);

	/** Updates the last screenshot stored in mDevices[] for the specified device.
	If the device is not in mDevices[], ignored. */
	void updateDeviceLastScreenshot(const QByteArray & aDeviceID, const QImage & aScreenshot);
};
