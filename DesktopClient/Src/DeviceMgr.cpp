#include "DeviceMgr.hpp"
#include <QMutexLocker>
#include "Comm/ConnectionMgr.hpp"
#include "MultiLogger.hpp"





DeviceMgr::DeviceMgr(ComponentCollection & aComponents):
	ComponentSuper(aComponents),
	mLogger(aComponents.logger("DeviceMgr"))
{
	requireForStart(ComponentCollection::ckConnectionMgr);
}





void DeviceMgr::start()
{
	auto connMgr = mComponents.get<ConnectionMgr>();
	connect(connMgr.get(), &ConnectionMgr::newConnection, this, &DeviceMgr::newConnection);
}





void DeviceMgr::addDevice(DevicePtr aDevice)
{
	auto & devID = aDevice->deviceID();
	QMutexLocker lock(&mMtx);
	auto itr = mDevices.find(devID);
	if (itr != mDevices.end())
	{
		throw DeviceAlreadyPresentError(mLogger, "Device (ID %1) already present", devID);
	}
	mDevices[devID] = aDevice;
	mLogger.log("Added new device: %1", devID);
	lock.unlock();
	emit deviceAdded(aDevice);
}





void DeviceMgr::delDevice(DevicePtr aDevice)
{
	QMutexLocker lock(&mMtx);
	auto itr = mDevices.find(aDevice->deviceID());
	if (itr == mDevices.end())
	{
		return;
	}
	mDevices.erase(itr);
	mLogger.log("Removed device %1", aDevice->deviceID());
	lock.unlock();
	emit deviceRemoved(aDevice);
}





std::vector<DevicePtr> DeviceMgr::devices() const
{
	std::vector<DevicePtr> res;
	QMutexLocker lock(&mMtx);
	for (const auto & dev: mDevices)
	{
		res.push_back(dev.second);
	}
	return res;
}





void DeviceMgr::newConnection(ConnectionPtr aConnection)
{
	mLogger.log("A new connection has arrived, ID %1, RemotePublicID %2",
		aConnection->connectionID(),
		aConnection->remotePublicID().value()
	);

	// Check if the device is already connected via another connection:
	{
		QMutexLocker lock(&mMtx);
		auto itr = mDevices.find(aConnection->remotePublicID().value());
		if (itr != mDevices.end())
		{
			mLogger.log("Device %1 is already tracked, adding the connection %2 to it.", itr->second->deviceID(), aConnection->connectionID());
			itr->second->addConnection(aConnection);
			return;
		}
	}

	// No such device yet, create a new one:
	mLogger.log("Creating a new device for connection %1, DeviceID = %2", aConnection->connectionID(), aConnection->remotePublicID().value());
	auto dev = std::make_shared<Device>(mComponents, aConnection->remotePublicID().value());
	dev->addConnection(aConnection);
	connect(dev.get(), &Device::goingOffline, this,
		[this, dev]()
		{
			delDevice(dev);
		}
	);
	addDevice(dev);
}
