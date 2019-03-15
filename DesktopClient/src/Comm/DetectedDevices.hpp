#pragma once

#include <memory>
#include <QAbstractTableModel>
#include <QImage>





/** Container for all devices detected by a single Enumerator.
Used both as a UI data holder and as a threading bridge between the enumerator threads and the main UI threads.
Various enumerators fill this container with the detected devices.
A device stored here has a specific ID that needn't be a regular DeviceID, but instead is an identifier
assigned and understood by the enumerator.
The assumption is that this object's thread is the only one doing reads and writes, other threads communicate
via invoking (the public functions invoke the protected functions on this object's thread. Therefore no
locking is needed for the data structures. */
class DetectedDevices:
	public QAbstractTableModel
{
	using Super = QAbstractTableModel;

	Q_OBJECT


public:

	/** The columns provided by this model. */
	enum
	{
		colDevice,      ///< The device's Name, or EnumeratorID if name not available
		colStatus,      ///< The status of the device
		colScreenshot,  ///< Screeshot of the device, if available

		colMax,
	};


	/** Encapsulates data for a single device. */
	class Device
	{
	public:
		enum Status
		{
			dsOnline,        ///< The device is working properly
			dsUnauthorized,  ///< The device requires on-device authorization before it can be accessed
			dsOffline,       ///< The device is known but unavailable (ADB bootloader, ...)
		};

		Device(const QByteArray & aEnumeratorDeviceID, Status aStatus);

		// Simple getters:
		const QByteArray & enumeratorDeviceID() const { return mEnumeratorDeviceID; }
		Status status() const { return mStatus; }
		const QImage & lastScreenshot() const { return mLastScreenshot; }
		const QString & name() const { return mName; }

		// Simple setters:
		void setStatus(Status aStatus) { mStatus = aStatus; }
		void setLastScreenshot(const QImage & aScreenshot) { mLastScreenshot = aScreenshot; }
		void setName(const QString & aName) { mName = aName; }


	protected:

		/** The Device's ID understood by the enumerator. */
		QByteArray mEnumeratorDeviceID;

		/** Current status of the device. */
		Status mStatus;

		/** The last screenshot taken of the device. */
		QImage mLastScreenshot;

		/** The name of the device (as set in the app), if known. */
		QString mName;
	};

	using DevicePtr = std::shared_ptr<Device>;

	/** A simple container for EnumeratorDeviceID + Status pairs. */
	using DeviceStatusList = std::vector<std::pair<QByteArray, Device::Status>>;


	/** Creates a new instance of the class. */
	explicit DetectedDevices(QObject * aParent = nullptr);


	// Cross-thread modificators to be called from the Enumerators
	// These can be called from a different thread (they internally invoke the actual implementation)

	/** Creates a new device with the specified ID and status.
	If such a device already exists, its status is updated instead. */
	void addDevice(const QByteArray & aEnumeratorDeviceID, Device::Status aStatus);

	/** Deletes the device with the specified ID.
	Ignored if such a device doesn't exist. */
	void delDevice(const QByteArray & aEnumeratorDeviceID);

	/** Updates the specified device's status.
	If such a device doesn't exist, creates it. */
	void setDeviceStatus(const QByteArray & aEnumeratorDeviceID, Device::Status aStatus);

	/** Updates the last screenshot for the specified device.
	Ignored if such a device doesn't exist. */
	void setDeviceScreenshot(const QByteArray & aEnumeratorDeviceID, const QImage & aScreenshot);

	/** Updates the entire device list.
	All items in mDevices that are not in aNewDeviceList are removed.
	All items in aNewDeviceList are either added or their status changed, appropriately. */
	void updateDeviceList(const DeviceStatusList & aNewDeviceList);


	// QAbstractTableModel overrides:
	virtual int rowCount(const QModelIndex & aParent) const override;
	virtual int columnCount(const QModelIndex & aParent) const override;
	virtual QVariant data(const QModelIndex & aIndex, int aRole) const override;
	virtual QVariant headerData(int aSection, Qt::Orientation aOrientation, int aRole) const override;


signals:

	/** Emitted when a new device is added to the list. */
	void deviceAdded(const QByteArray & aEnumeratorDeviceID, Device::Status aStatus);

	/** Emitted when the device's status changes. */
	void deviceStatusChanged(const QByteArray & aEnumeratorDeviceID, Device::Status aStatus);


public slots:

protected:

	/** All the devices, in no particular order (but still ordered, to make the model consistent). */
	std::vector<DevicePtr> mDevices;


	/** The actual implementation of addDevice, guaranteed to be invoked in this object's thread. */
	Q_INVOKABLE void invAddDevice(const QByteArray & aEnumeratorDeviceID, Device::Status aStatus);

	/** The actual implementation of delDevice, guaranteed to be invoked in this object's thread. */
	Q_INVOKABLE void invDelDevice(const QByteArray & aEnumeratorDeviceID);

	/** The actual implementation of setDeviceStatus, guaranteed to be invoked in this object's thread. */
	Q_INVOKABLE void invSetDeviceStatus(const QByteArray & aEnumeratorDeviceID, Device::Status aStatus);

	/** The actual implementation of setDeviceScreenshot, guaranteed to be invoked in this object's thread. */
	Q_INVOKABLE void invSetDeviceScreenshot(const QByteArray & aEnumeratorDeviceID, const QImage & aScreenshot);

	/** The actual implementation of updateDeviceList, guaranteed to be invoked in this object's thread. */
	Q_INVOKABLE void invUpdateDeviceList(const DetectedDevices::DeviceStatusList & aNewDeviceList);
};

Q_DECLARE_METATYPE(DetectedDevices::DeviceStatusList);
