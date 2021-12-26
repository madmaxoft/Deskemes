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
	ComponentSuper(aComponents)
{

}





void DetectedDevices::addDevice(ComponentCollection::ComponentKind aEnumeratorKind, const QByteArray & aEnumeratorDeviceID, DetectedDevices::Device::Status aStatus)
{
	QMutexLocker lock(&mtxDevices);
	qDebug() << "Adding device " << aEnumeratorDeviceID;

	// If the device is already present, only update status:
	for (auto & dev: mDevices)
	{
		if (dev->enumeratorDeviceID() != aEnumeratorDeviceID)
		{
			continue;
		}
		if (dev->status() == aStatus)
		{
			return;
		}
		dev->setStatus(aStatus);
		lock.unlock();
		emit deviceStatusChanged(dev);
		return;
	}

	// Device not found, create a new one:
	auto dev = std::make_shared<Device>(aEnumeratorKind, aEnumeratorDeviceID, aStatus);
	mDevices.push_back(dev);
	lock.unlock();
	emit deviceAdded(dev);
}





void DetectedDevices::delDevice(const QByteArray & aEnumeratorDeviceID)
{
	QMutexLocker lock(&mtxDevices);
	qDebug() << "Removing device " << aEnumeratorDeviceID;

	int row = -1;
	for (auto & dev: mDevices)
	{
		row += 1;
		if (dev->enumeratorDeviceID() != aEnumeratorDeviceID)
		{
			continue;
		}
		mDevices.erase(mDevices.begin() + row);
		lock.unlock();
		emit deviceRemoved(dev);
		return;
	}

	qDebug() << "Device " << aEnumeratorDeviceID << " not found";
}





void DetectedDevices::setDeviceStatus(
	ComponentCollection::ComponentKind aEnumeratorKind,
	const QByteArray & aEnumeratorDeviceID,
	DetectedDevices::Device::Status aStatus
)
{
	addDevice(aEnumeratorKind, aEnumeratorDeviceID, aStatus);
}





void DetectedDevices::setDeviceName(const QByteArray & aEnumeratorDeviceID, const QString & aName)
{
	QMutexLocker lock(&mtxDevices);
	for (auto & dev: mDevices)
	{
		if (dev->enumeratorDeviceID() != aEnumeratorDeviceID)
		{
			continue;
		}
		dev->setName(aName);
		lock.unlock();
		emit deviceNameChanged(dev);
		return;
	}

	qDebug() << "Device " << aEnumeratorDeviceID << " not found";
}





void DetectedDevices::setDeviceAvatar(const QByteArray & aEnumeratorDeviceID, const QImage & aAvatar)
{
	QMutexLocker lock(&mtxDevices);
	for (auto & dev: mDevices)
	{
		if (dev->enumeratorDeviceID() != aEnumeratorDeviceID)
		{
			continue;
		}
		dev->setAvatar(aAvatar);
		lock.unlock();
		emit deviceAvatarChanged(dev);
		return;
	}

	qDebug() << "Device " << aEnumeratorDeviceID << " not found";
}





void DetectedDevices::setDeviceAvatar(const QByteArray & aEnumeratorDeviceID, const QByteArray & aAvatarImgData)
{
	auto avatar = QImage::fromData(aAvatarImgData);
	if (avatar.isNull())
	{
		qDebug() << "Avatar data cannot be decoded into an image for device " << aEnumeratorDeviceID;
		return;
	}
	setDeviceAvatar(aEnumeratorDeviceID, avatar);
}





void DetectedDevices::updateEnumeratorDeviceList(
	ComponentCollection::ComponentKind aEnumeratorKind,
	const DeviceStatusList & aNewDeviceList
)
{
	QMutexLocker lock(&mtxDevices);

	// Remove all devices no longer known:
	std::set<QByteArray> devicesToRemove;
	for (const auto & dev: mDevices)
	{
		if (dev->enumeratorKind() == aEnumeratorKind)
		{
			devicesToRemove.insert(dev->enumeratorDeviceID());
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





std::vector<DetectedDevices::DevicePtr> DetectedDevices::devices() const
{
	QMutexLocker lock(&mtxDevices);
	return mDevices;  // Returns a copy of mDevices
}
