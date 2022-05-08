#pragma once

#include <memory>
#include <QAbstractTableModel>
#include <QImage>
#include <QMutex>

#include "../ComponentCollection.hpp"





/** Container for all devices detected by all Enumerators.
Various enumerators fill this container with the detected devices.
A device stored here has a specific ID that needn't be a regular DeviceID, but instead is an identifier
assigned and understood by the enumerator.
The functions in this object are thread-safe, so they can be safely called from other threads in parallel;
this object then serializes the changes in the device list and provides them further down to the other
parts of the system, using Qt signals. */
class DetectedDevices:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckDetectedDevices>
{
	using ComponentSuper = ComponentCollection::Component<ComponentCollection::ckDetectedDevices>;

	Q_OBJECT

	// ComponentCollection::Component overrides:
	virtual void start(void) override {}  // Nothing needed


public:

	/** Encapsulates data for a single device. */
	class Device
	{
	public:
		enum Status
		{
			dsOnline,        ///< The device is working properly (fully connected)
			dsNoPubKey,      ///< The device hasn't been paired yet (just now discovered, have no pubkey)
			dsNeedPairing,   ///< The device has provided a PublicKey but it's not trusted, need user's confirmation
			dsUnauthorized,  ///< The device requires on-device (ADB) authorization before it can be accessed
			dsBlacklisted,   ///< The device has been blacklisted, connections will not be allowed
			dsOffline,       ///< The device is known but unavailable (ADB bootloader, ...)
			dsNeedApp,       ///< The app is not installed on the device (ADB)
			dsFailed,        ///< The device failed to connect properly, unknown reason (eg. ADB port-reversing failed)
		};

		Device(ComponentCollection::ComponentKind aEnumeratorKind, const QByteArray & aEnumeratorDeviceID, Status aStatus);

		// Simple getters:
		ComponentCollection::ComponentKind enumeratorKind() const { return mEnumeratorKind; }
		const QByteArray & enumeratorDeviceID() const { return mEnumeratorDeviceID; }
		Status status() const { return mStatus; }
		const QImage & avatar() const { return mAvatar; }
		const QString & name() const { return mName; }

		// Simple setters:
		void setStatus(Status aStatus) { mStatus = aStatus; }
		void setAvatar(const QImage & aAvatar) { mAvatar = aAvatar; }
		void setName(const QString & aName) { mName = aName; }


	protected:

		/** The enumerator that created this device. */
		ComponentCollection::ComponentKind mEnumeratorKind;

		/** The Device's ID understood by the enumerator. */
		QByteArray mEnumeratorDeviceID;

		/** The ID of the connection that has been made to the device (available in ConnectionMgr). */
		QByteArray mConnectionID;

		/** Current status of the device. */
		Status mStatus;

		/** The name of the device (as set in the app), if known. */
		QString mName;

		/** The avatar representing the device. */
		QImage mAvatar;
	};

	using DevicePtr = std::shared_ptr<Device>;

	/** A map of {EnumeratorKind, EnumeratorID} -> DevicePtr */
	using DevicePtrMap = std::map<std::pair<ComponentCollection::ComponentKind, QByteArray>, DevicePtr>;

	/** A simple container for EnumeratorDeviceID + Status tuples. */
	using DeviceStatusList = std::vector<std::pair<QByteArray, Device::Status>>;


	/** Creates a new instance of the class. */
	explicit DetectedDevices(ComponentCollection & aComponents);


	// Cross-thread modificators to be called from the Enumerators
	// These can be called from a different thread, they lock the data structures they work upon.

	/** Creates a new device with the specified ID and status.
	If such a device already exists, its status is updated instead. */
	void addDevice(ComponentCollection::ComponentKind aEnumeratorKind, const QByteArray & aEnumeratorDeviceID, Device::Status aStatus);

	/** Deletes the device with the specified ID.
	Ignored if such a device doesn't exist. */
	void delDevice(const QByteArray & aEnumeratorDeviceID);

	/** Updates the specified device's status.
	If such a device doesn't exist, creates it. */
	void setDeviceStatus(ComponentCollection::ComponentKind aEnumeratorKind, const QByteArray & aEnumeratorDeviceID, Device::Status aStatus);

	/** Updates the specified device's name.
	Ignored if such a device doesn't exist. */
	void setDeviceName(const QByteArray & aEnumeratorDeviceID, const QString & aName);

	/** Updates the avatar for the specified device.
	Ignored if such a device doesn't exist. */
	void setDeviceAvatar(const QByteArray & aEnumeratorDeviceID, const QImage & aAvatar);

	/** Updates the avatar for the specified device, based on the binary avatar data (JPG or PNG).
	Ignored if such a device doesn't exist, or if the avatar data cannot be decoded into an image. */
	void setDeviceAvatar(const QByteArray & aEnumeratorDeviceID, const QByteArray & aAvatarImgData);

	/** Updates the complete device list for the specified enumerator.
	All enumerator's items in mDevices that are not in aNewDeviceList are removed.
	All items in aNewDeviceList are either added or their status changed, appropriately. */
	void updateEnumeratorDeviceList(ComponentCollection::ComponentKind aEnumeratorKind, const DeviceStatusList & aNewDeviceList);

	/** Returns all the devices currently tracked (a copy of mDevices). */
	DevicePtrMap devices() const;

	/** Returns all of the devices currently tracked by the specified enumerator. */
	std::map<QByteArray, DevicePtr> allEnumeratorDevices(ComponentCollection::ComponentKind aEnumeratorKind) const;


Q_SIGNALS:

	/** Emitted just after a new device is added to the list. */
	void deviceAdded(DetectedDevices::DevicePtr aDevice);

	/** Emitted just before a device is removed from the list. */
	void deviceRemoved(DetectedDevices::DevicePtr aDevice);

	/** Emitted when the device's status changes. */
	void deviceStatusChanged(DetectedDevices::DevicePtr aDevice);

	/** Emitted when the device's avatar changes. */
	void deviceAvatarChanged(DetectedDevices::DevicePtr aDevice);

	/** Emitted when the device's name changes. */
	void deviceNameChanged(DetectedDevices::DevicePtr aDevice);


protected:

	/** The mutex protecting mDevices against multithreaded access. */
	mutable QRecursiveMutex mtxDevices;

	/** All the devices, in no particular order, protected by mMtxDevices. */
	DevicePtrMap mDevices;

	/** The logger used for all messages produced by this class. */
	Logger & mLogger;
};

Q_DECLARE_METATYPE(DetectedDevices::DevicePtr);
