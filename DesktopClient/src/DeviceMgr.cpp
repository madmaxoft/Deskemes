#include "DeviceMgr.hpp"
#include <QMutexLocker>





DeviceMgr::DeviceMgr(ComponentCollection & aComponents):
	mComponents(aComponents)
{
}





void DeviceMgr::addDevice(DevicePtr aDevice)
{
	auto & devID = aDevice->deviceID();
	QMutexLocker lock(&mMtx);
	auto itr = mDevices.find(devID);
	if (itr != mDevices.end())
	{
		throw DeviceAlreadyPresentError("Device already present");
	}
	mDevices[devID] = aDevice;
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
	qDebug() << "A new connection has arrived: " << aConnection->connectionID();

	// Check if the device is already connected via another connection:
	{
		QMutexLocker lock(&mMtx);
		auto itr = mDevices.find(aConnection->remotePublicID().value());
		if (itr != mDevices.end())
		{
			qDebug() << "Device " << itr->second->deviceID() << " is already tracked, adding the connection to it.";
			itr->second->addConnection(aConnection);
			return;
		}
	}

	// No such device yet, create a new one:
	qDebug() << "Creating a new device for connection " << aConnection->connectionID()
		<< "; DeviceID = " << aConnection->remotePublicID().value();
	auto dev = std::make_shared<Device>(aConnection->remotePublicID().value());
	dev->addConnection(aConnection);
	connect(dev.get(), &Device::goingOffline,
		[this, dev]()
		{
			delDevice(dev);
		}
	);
	addDevice(dev);
}
