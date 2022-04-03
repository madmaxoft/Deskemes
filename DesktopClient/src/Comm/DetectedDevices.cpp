#include "DetectedDevices.hpp"
#include <cassert>
#include <set>
#include <QDebug>





////////////////////////////////////////////////////////////////////////////////
// DetectedDevices::Device:

DetectedDevices::Device::Device(
	ComponentCollection::ComponentKind aEnumeratorKind,
	const QByteArray & aEnumeratorDeviceID,
	DetectedDevices::Device::Status aStatus
):
	mEnumeratorKind(aEnumeratorKind),
	mEnumeratorDeviceID(aEnumeratorDeviceID),
	mStatus(aStatus)
{
}





////////////////////////////////////////////////////////////////////////////////
// DetectedDevices:

DetectedDevices::DetectedDevices(ComponentCollection & aComponents):
	ComponentSuper(aComponents),
	mLogger(aComponents.logger("DetectedDevices"))
{

}





void DetectedDevices::addDevice(ComponentCollection::ComponentKind aEnumeratorKind, const QByteArray & aEnumeratorDeviceID, DetectedDevices::Device::Status aStatus)
{
	QMutexLocker lock(&mtxDevices);
	mLogger.log("Adding device %1, status %2.", aEnumeratorDeviceID, aStatus);

	// If the device is already present (no matter what enumerator from), only update status:
	for (auto & devEntry: mDevices)
	{
		if (devEntry.first.second != aEnumeratorDeviceID)
		{
			continue;
		}
		if (devEntry.second->status() == aStatus)
		{
			mLogger.log("Device %1 already present with the same status, ignoring.", aEnumeratorDeviceID);
			return;
		}
		mLogger.log("Device %1 already present, updating status from %2 to %3", aEnumeratorDeviceID, devEntry.second->status(), aStatus);
		devEntry.second->setStatus(aStatus);
		lock.unlock();
		emit deviceStatusChanged(devEntry.second);
		return;
	}

	// Device not found, create a new one:
	mLogger.log("Device %1 not found, creating a new one.", aEnumeratorDeviceID);
	auto dev = std::make_shared<Device>(aEnumeratorKind, aEnumeratorDeviceID, aStatus);
	mDevices[{aEnumeratorKind, aEnumeratorDeviceID}] = dev;
	lock.unlock();
	emit deviceAdded(dev);
}





void DetectedDevices::delDevice(const QByteArray & aEnumeratorDeviceID)
{
	QMutexLocker lock(&mtxDevices);
	mLogger.log("Removing device %1.", aEnumeratorDeviceID);

	for (auto itr = mDevices.begin(), end = mDevices.end(); itr != end; ++itr)
	{
		if (itr->first.second != aEnumeratorDeviceID)
		{
			continue;
		}
		auto dev = itr->second;
		mDevices.erase(itr);
		lock.unlock();
		emit deviceRemoved(dev);
		return;
	}
	mLogger.log("Device %1 not found.", aEnumeratorDeviceID);
}





void DetectedDevices::setDeviceStatus(
	ComponentCollection::ComponentKind aEnumeratorKind,
	const QByteArray & aEnumeratorDeviceID,
	DetectedDevices::Device::Status aStatus
)
{
	// No need to log anything here, the addDevice() logs extensively.
	addDevice(aEnumeratorKind, aEnumeratorDeviceID, aStatus);
}





void DetectedDevices::setDeviceName(const QByteArray & aEnumeratorDeviceID, const QString & aName)
{
	mLogger.log("Device id %1 is changing name to %2.", aEnumeratorDeviceID, aName);
	QMutexLocker lock(&mtxDevices);
	for (auto & devEntry: mDevices)
	{
		if (devEntry.first.second != aEnumeratorDeviceID)
		{
			continue;
		}
		devEntry.second->setName(aName);
		lock.unlock();
		emit deviceNameChanged(devEntry.second);
		return;
	}

	mLogger.log("Device %1 not found.", aEnumeratorDeviceID);
}





void DetectedDevices::setDeviceAvatar(const QByteArray & aEnumeratorDeviceID, const QImage & aAvatar)
{
	mLogger.log("Device id %1 is changing its avatar.", aEnumeratorDeviceID);
	QMutexLocker lock(&mtxDevices);
	for (auto & devEntry: mDevices)
	{
		if (devEntry.first.second != aEnumeratorDeviceID)
		{
			continue;
		}
		devEntry.second->setAvatar(aAvatar);
		lock.unlock();
		emit deviceAvatarChanged(devEntry.second);
		return;
	}

	mLogger.log("Device %1 not found.", aEnumeratorDeviceID);
}





void DetectedDevices::setDeviceAvatar(const QByteArray & aEnumeratorDeviceID, const QByteArray & aAvatarImgData)
{
	auto avatar = QImage::fromData(aAvatarImgData);
	if (avatar.isNull())
	{
		mLogger.log("Avatar data cannot be decoded into an image for device %1.", aEnumeratorDeviceID);
		return;
	}
	setDeviceAvatar(aEnumeratorDeviceID, avatar);
}





void DetectedDevices::updateEnumeratorDeviceList(
	ComponentCollection::ComponentKind aEnumeratorKind,
	const DeviceStatusList & aNewDeviceList
)
{
	mLogger.log("Enumerator %1 is updating its devices, has a list of %2 devices.", aEnumeratorKind, aNewDeviceList.size());
	QMutexLocker lock(&mtxDevices);

	// Remove all devices no longer known:
	std::set<QByteArray> devicesToRemove;
	for (const auto & devEntry: mDevices)
	{
		if (devEntry.first.first == aEnumeratorKind)
		{
			devicesToRemove.insert(devEntry.first.second);
		}
	}
	for (const auto & nd: aNewDeviceList)
	{
		devicesToRemove.erase(nd.first);
	}
	for (const auto & rd: devicesToRemove)
	{
		delDevice(rd);
	}

	// Add all devices not known yet:
	for (const auto & nd: aNewDeviceList)
	{
		addDevice(aEnumeratorKind, nd.first, nd.second);
	}
}





DetectedDevices::DevicePtrMap DetectedDevices::devices() const
{
	QMutexLocker lock(&mtxDevices);
	return mDevices;  // Returns a copy of mDevices
}





std::map<QByteArray, DetectedDevices::DevicePtr> DetectedDevices::allEnumeratorDevices(ComponentCollection::ComponentKind aEnumeratorKind) const
{
	QMutexLocker lock(&mtxDevices);
	std::map<QByteArray, DetectedDevices::DevicePtr> res;
	for (const auto & devEntry: mDevices)
	{
		if (devEntry.first.first == aEnumeratorKind)
		{
			res[devEntry.first.second] = devEntry.second;
		}
	}
	return res;
}
