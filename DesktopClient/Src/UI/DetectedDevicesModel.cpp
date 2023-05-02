#include "DetectedDevicesModel.hpp"





namespace
{

/** A singleton containing the static data used by the model - the images. */
class StaticData
{
	StaticData():
		mImgUsb(":/img/usb.png"),
		mImgBluetooth(":/img/bluetooth.png"),
		mImgTcp(":/img/wifi.png")
	{
	}


public:
	QImage mImgUsb;
	QImage mImgBluetooth;
	QImage mImgTcp;

	static StaticData & get()
	{
		static StaticData theInstance;
		return theInstance;
	}
};
}





DetectedDevicesModel::DetectedDevicesModel(ComponentCollection & aComponents, QObject * aParent):
	Super(aParent),
	mComponents(aComponents)
{
	// Bind to the ckDetectedDevices' callbacks:
	auto dd = aComponents.get<DetectedDevices>();
	connect(dd.get(), &DetectedDevices::deviceAdded,         this, &DetectedDevicesModel::addDevice,          Qt::QueuedConnection);
	connect(dd.get(), &DetectedDevices::deviceRemoved,       this, &DetectedDevicesModel::removeDevice,       Qt::QueuedConnection);
	connect(dd.get(), &DetectedDevices::deviceStatusChanged, this, &DetectedDevicesModel::updateDeviceStatus, Qt::QueuedConnection);
	connect(dd.get(), &DetectedDevices::deviceAvatarChanged, this, &DetectedDevicesModel::updateDeviceAvatar, Qt::QueuedConnection);
	connect(dd.get(), &DetectedDevices::deviceNameChanged,   this, &DetectedDevicesModel::updateDeviceName,   Qt::QueuedConnection);

	// Read the currently-present devices:
	auto currentDevices = dd->devices();
	for (const auto & dev: currentDevices)
	{
		mDevices.push_back(dev.second);
	}
}





DetectedDevicesModel::~DetectedDevicesModel()
{
	// Disconnect from all the signals:
	disconnect();
}




int DetectedDevicesModel::rowCount(const QModelIndex & aParent) const
{
	if (aParent.isValid())
	{
		assert(!"Tables shouldn't have a parent");
		return 0;
	}
	return static_cast<int>(mDevices.size());
}





int DetectedDevicesModel::columnCount(const QModelIndex & aParent) const
{
	Q_UNUSED(aParent);
	return colMax;
}





QVariant DetectedDevicesModel::data(const QModelIndex & aIndex, int aRole) const
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
		case roleDevPtr:
		{
			return QVariant::fromValue(device);
		}
		case Qt::DisplayRole:
		{
			switch (aIndex.column())
			{
				case colDeviceName:
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
						case DetectedDevices::Device::dsOnline:       return tr("Online");
						case DetectedDevices::Device::dsNoPubKey:     return tr("Not paired");
						case DetectedDevices::Device::dsNeedPairing:  return tr("Need pairing");
						case DetectedDevices::Device::dsUnauthorized: return tr("Authorization needed");
						case DetectedDevices::Device::dsOffline:      return tr("Offline");
						case DetectedDevices::Device::dsBlacklisted:  return tr("Blacklisted");
						case DetectedDevices::Device::dsNeedApp:      return tr("App not installed");
						case DetectedDevices::Device::dsFailed:       return tr("Connection failed");
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
				case colDeviceName:
				{
					switch (device->enumeratorKind())
					{
						case ComponentCollection::ckUsbDeviceEnumerator:       return StaticData::get().mImgUsb;
						case ComponentCollection::ckBluetoothDeviceEnumerator: return StaticData::get().mImgBluetooth;
						case ComponentCollection::ckTcpListener:               return StaticData::get().mImgTcp;
						default: return QVariant();
					}
				}
				case colAvatar:
				{
					return device->avatar();
				}
			}
			break;
		}
	}
	return {};
}





QVariant DetectedDevicesModel::headerData(int aSection, Qt::Orientation aOrientation, int aRole) const
{
	if ((aOrientation != Qt::Horizontal) || (aRole != Qt::DisplayRole))
	{
		return {};
	}
	switch (aSection)
	{
		case colDeviceName:  return tr("Device");
		case colStatus:      return tr("Status");
		case colAvatar:      return tr("Avatar");
	}
	return {};
}





int DetectedDevicesModel::indexOfDevice(DetectedDevices::DevicePtr aDevice)
{
	int idx = 0;
	for (const auto & dev: mDevices)
	{
		if (dev == aDevice)
		{
			return idx;
		}
		++idx;
	}

	throw Exception("Device not in the list");
}





void DetectedDevicesModel::addDevice(DetectedDevices::DevicePtr aDevice)
{
	auto idx = static_cast<int>(mDevices.size());
	beginInsertRows({}, idx, idx);
	mDevices.push_back(aDevice);
	endInsertRows();
}





void DetectedDevicesModel::removeDevice(DetectedDevices::DevicePtr aDevice)
{
	auto idx = indexOfDevice(aDevice);
	beginRemoveRows({}, idx, idx);
	mDevices.erase(mDevices.begin() + idx);
	endRemoveRows();
}





void DetectedDevicesModel::updateDeviceStatus(DetectedDevices::DevicePtr aDevice)
{
	auto idx = index(indexOfDevice(aDevice), colStatus);
	emit dataChanged(idx, idx);
}





void DetectedDevicesModel::updateDeviceAvatar(DetectedDevices::DevicePtr aDevice)
{
	auto idx = index(indexOfDevice(aDevice), colAvatar);
	emit dataChanged(idx, idx);
}





void DetectedDevicesModel::updateDeviceName(DetectedDevices::DevicePtr aDevice)
{
	auto idx = index(indexOfDevice(aDevice), colDeviceName);
	emit dataChanged(idx, idx);
}
