#include "Devices.hpp"
#include <QMutexLocker>





Devices::Devices(ComponentCollection & aComponents):
	mComponents(aComponents)
{
	// DEBUG: Add a dummy device for testing purposes:
	addDevice(std::make_shared<Device>("dummyDevice"));
}





void Devices::addDevice(DevicePtr aDevice)
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





void Devices::delDevice(DevicePtr aDevice)
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





std::vector<DevicePtr> Devices::devices() const
{
	std::vector<DevicePtr> res;
	QMutexLocker lock(&mMtx);
	for (const auto & dev: mDevices)
	{
		res.push_back(dev.second);
	}
	return res;
}




