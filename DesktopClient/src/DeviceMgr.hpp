#pragma once

#include <map>
#include <string>
#include <QObject>
#include <QMutex>
#include "ComponentCollection.hpp"
#include "Device.hpp"
#include "Comm/Connection.hpp"





/** Manages all the devices known to the app. */
class DeviceMgr:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckDevices>
{
	using ComponentSuper = ComponentCollection::Component<ComponentCollection::ckDevices>;

	Q_OBJECT


public:

	class DeviceAlreadyPresentError: public RuntimeError
	{
		using RuntimeError::RuntimeError;
	};

	DeviceMgr(ComponentCollection & aComponents);

	virtual void start() override;

	/** Adds the specified device to the internal storage.
	Throws a DeviceAlreadyPresentError if such a device with the same DeviceID already exists. */
	void addDevice(DevicePtr aDevice);

	/** Removes the specified device from the internal storage.
	Ignored if the device is not in the storage. */
	void delDevice(DevicePtr aDevice);

	/** Returns all the devices currently stored. */
	std::vector<DevicePtr> devices() const;


protected:

	/** All the devices, indexed by their DeviceID.
	Protected against multithreaded access by mMtx. */
	std::map<QByteArray, DevicePtr> mDevices;

	/** Protects mDevices against multithreaded access. */
	mutable QMutex mMtx;


Q_SIGNALS:

	/** Emitted after a new device is added to the internal storage. */
	void deviceAdded(DevicePtr aDevice);

	/** Emitted after a device was removed from the internal storage. */
	void deviceRemoved(DevicePtr aDevice);


public Q_SLOTS:

	/** Notification from ConnectionMgr, a new connection has reached full connectivity (csEncrypted).
	Either creates a new device for the connection, or adds the connection to an existing device. */
	void newConnection(ConnectionPtr aConnection);
};
