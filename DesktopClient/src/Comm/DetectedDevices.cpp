#include "DetectedDevices.hpp"
#include <cassert>
#include <set>
#include <QDebug>





////////////////////////////////////////////////////////////////////////////////
// DetectedDevices::Device:

DetectedDevices::Device::Device(
	const QByteArray & aEnumeratorDeviceID,
	DetectedDevices::Device::Status aStatus
):
	mEnumeratorDeviceID(aEnumeratorDeviceID),
	mStatus(aStatus)
{
}





////////////////////////////////////////////////////////////////////////////////
// DetectedDevices:

DetectedDevices::DetectedDevices(QObject * aParent):
	Super(aParent)
{

}





void DetectedDevices::addDevice(const QByteArray & aEnumeratorDeviceID, DetectedDevices::Device::Status aStatus)
{
	QMetaObject::invokeMethod(
		this, "invAddDevice",
		Qt::AutoConnection,
		Q_ARG(const QByteArray &, aEnumeratorDeviceID),
		Q_ARG(DetectedDevices::Device::Status, aStatus)
	);
}





void DetectedDevices::delDevice(const QByteArray & aEnumeratorDeviceID)
{
	QMetaObject::invokeMethod(
		this, "invDelDevice",
		Qt::AutoConnection,
		Q_ARG(const QByteArray &, aEnumeratorDeviceID)
	);
}





void DetectedDevices::setDeviceStatus(const QByteArray & aEnumeratorDeviceID, DetectedDevices::Device::Status aStatus)
{
	QMetaObject::invokeMethod(
		this, "invSetDeviceStatus",
		Qt::AutoConnection,
		Q_ARG(const QByteArray &, aEnumeratorDeviceID),
		Q_ARG(DetectedDevices::Device::Status, aStatus)
	);
}





void DetectedDevices::setDeviceName(const QByteArray & aEnumeratorDeviceID, const QString & aName)
{
	QMetaObject::invokeMethod(
		this, "invSetDeviceName",
		Qt::AutoConnection,
		Q_ARG(const QByteArray &, aEnumeratorDeviceID),
		Q_ARG(const QString &, aName)
	);
}





void DetectedDevices::setDeviceAvatar(const QByteArray & aEnumeratorDeviceID, const QImage & aAvatar)
{
	QMetaObject::invokeMethod(
		this, "invSetDeviceAvatar",
		Qt::AutoConnection,
		Q_ARG(const QByteArray &, aEnumeratorDeviceID),
		Q_ARG(const QImage &, aAvatar)
	);
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





void DetectedDevices::updateDeviceList(const DeviceStatusList & aNewDeviceList
)
{
	QMetaObject::invokeMethod(
		this, "invUpdateDeviceList",
		Qt::AutoConnection,
		Q_ARG(DetectedDevices::DeviceStatusList, aNewDeviceList)
	);
}





int DetectedDevices::rowCount(const QModelIndex & aParent) const
{
	if (aParent.isValid())
	{
		assert(!"Tables shouldn't have a parent");
		return 0;
	}
	return static_cast<int>(mDevices.size());
}





int DetectedDevices::columnCount(const QModelIndex & aParent) const
{
	Q_UNUSED(aParent);
	return colMax;
}





QVariant DetectedDevices::data(const QModelIndex & aIndex, int aRole) const
{
	if (!aIndex.isValid())
	{
		assert(!"Invalid index");
		return {};
	}
	auto row = aIndex.row();
	if ((row < 0) || (static_cast<size_t>(row) >= mDevices.size()))
	{
		assert(!"Invalid row");
		return {};
	}
	const auto & device = mDevices[static_cast<size_t>(row)];
	switch (aRole)
	{
		case Qt::DisplayRole:
		{
			switch (aIndex.column())
			{
				case colDevice:
				{
					if (device->name().isEmpty())
					{
						return QString::fromUtf8(device->enumeratorDeviceID());
					}
					else
					{
						return device->name();
					}
				}
				case colStatus:
				{
					switch (device->status())
					{
						case Device::dsOnline:       return tr("Online");
						case Device::dsNoPubKey:     return tr("Not paired");
						case Device::dsNeedPairing:  return tr("Need pairing");
						case Device::dsUnauthorized: return tr("Authorization needed");
						case Device::dsOffline:      return tr("Offline");
					}
					return {};
				}
			}
			break;
		}		// case Qt::DisplayRole

		case Qt::DecorationRole:
		{
			switch (aIndex.column())
			{
				case colAvatar: return device->avatar();
			}
			break;
		}
	}
	return {};
}





QVariant DetectedDevices::headerData(int aSection, Qt::Orientation aOrientation, int aRole) const
{
	if ((aOrientation != Qt::Horizontal) || (aRole != Qt::DisplayRole))
	{
		return {};
	}
	switch (aSection)
	{
		case colDevice:  return tr("Device");
		case colStatus:  return tr("Status");
		case colAvatar:  return tr("Avatar");
	}
	return {};
}





void DetectedDevices::invAddDevice(const QByteArray & aEnumeratorDeviceID, DetectedDevices::Device::Status aStatus)
{
	// If the device is already present, only update status:
	int row = -1;
	for (auto & dev: mDevices)
	{
		row += 1;
		if (dev->enumeratorDeviceID() != aEnumeratorDeviceID)
		{
			continue;
		}
		if (dev->status() == aStatus)
		{
			return;
		}
		dev->setStatus(aStatus);
		auto idx = index(row, colStatus);
		emit deviceStatusChanged(aEnumeratorDeviceID, aStatus);
		emit dataChanged(idx, idx);
	}

	// Device not found, create a new one:
	auto dev = std::make_shared<Device>(aEnumeratorDeviceID, aStatus);
	auto numDevices = static_cast<int>(mDevices.size());
	beginInsertRows({}, numDevices, numDevices);
	mDevices.push_back(dev);
	endInsertRows();
	emit deviceAdded(aEnumeratorDeviceID, aStatus);
}





void DetectedDevices::invDelDevice(const QByteArray & aEnumeratorDeviceID)
{
	int row = -1;
	for (auto & dev: mDevices)
	{
		row += 1;
		if (dev->enumeratorDeviceID() != aEnumeratorDeviceID)
		{
			continue;
		}
		beginRemoveRows({}, row, row);
		mDevices.erase(mDevices.begin() + row);
		endRemoveRows();
		return;
	}

	qDebug() << "Device " << aEnumeratorDeviceID << " not found";
}





void DetectedDevices::invSetDeviceStatus(const QByteArray & aEnumeratorDeviceID, DetectedDevices::Device::Status aStatus)
{
	invAddDevice(aEnumeratorDeviceID, aStatus);
}





void DetectedDevices::invSetDeviceName(const QByteArray & aEnumeratorDeviceID, const QString & aName)
{
	int row = -1;
	for (auto & dev: mDevices)
	{
		row += 1;
		if (dev->enumeratorDeviceID() != aEnumeratorDeviceID)
		{
			continue;
		}
		dev->setName(aName);
		auto idx = index(row, colDevice);
		emit dataChanged(idx, idx);
		return;
	}

	qDebug() << "Device " << aEnumeratorDeviceID << " not found";
}





void DetectedDevices::invSetDeviceAvatar(const QByteArray & aEnumeratorDeviceID, const QImage & aAvatar)
{
	int row = -1;
	for (auto & dev: mDevices)
	{
		row += 1;
		if (dev->enumeratorDeviceID() != aEnumeratorDeviceID)
		{
			continue;
		}
		dev->setAvatar(aAvatar);
		auto idx = index(row, colAvatar);
		emit dataChanged(idx, idx);
		return;
	}

	qDebug() << "Device " << aEnumeratorDeviceID << " not found";
}





void DetectedDevices::invUpdateDeviceList(const DetectedDevices::DeviceStatusList & aNewDeviceList)
{
	// Remove all devices no longer known:
	std::set<QByteArray> devicesToRemove;
	for (const auto & dev: mDevices)
	{
		devicesToRemove.insert(dev->enumeratorDeviceID());
	}
	for (const auto & nd: aNewDeviceList)
	{
		devicesToRemove.erase(nd.first);
	}
	for (const auto & rd: devicesToRemove)
	{
		invDelDevice(rd);
	}

	// Add all devices not known yet:
	for (const auto & nd: aNewDeviceList)
	{
		invAddDevice(nd.first, nd.second);
	}
}
