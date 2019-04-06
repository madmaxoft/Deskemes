#include "ConnectionMgr.hpp"
#include <cassert>





ConnectionMgr::ConnectionMgr(ComponentCollection & aComponents):
	mComponents(aComponents)
{

}





std::vector<std::shared_ptr<Connection>> ConnectionMgr::connections() const
{
	QMutexLocker lock(&mMtxConnections);
	return mConnections;
}





std::shared_ptr<DetectedDevices> ConnectionMgr::detectDevices()
{
	auto dd = std::make_shared<DetectedDevices>();
	{
		QMutexLocker lock(&mMtxDetections);
		mDetections.push_back(dd);
	}
	connect(dd.get(), &DetectedDevices::destroyed, this, &ConnectionMgr::removeDetection);

	// Report all currently connected devices:
	{
		QMutexLocker lock(&mMtxConnections);
		for (const auto & conn: mConnections)
		{
			updateDetectedDevice(*dd, *conn);
		}
	}
	return dd;
}





ConnectionPtr ConnectionMgr::connectionFromID(const QByteArray & aConnectionID)
{
	QMutexLocker lock(&mMtxConnections);
	for (const auto & conn: mConnections)
	{
		if (conn->connectionID() == aConnectionID)
		{
			return conn;
		}
	}
	return nullptr;
}





void ConnectionMgr::addConnection(std::shared_ptr<Connection> aConnection)
{
	// Register the change notifications for the device detection:
	connect(aConnection.get(), &Connection::receivedPublicID,     this, &ConnectionMgr::connUpdateDetails);
	connect(aConnection.get(), &Connection::receivedFriendlyName, this, &ConnectionMgr::connUpdateDetails);
	connect(aConnection.get(), &Connection::receivedAvatar,       this, &ConnectionMgr::connUpdateDetails);
	connect(aConnection.get(), &Connection::stateChanged,         this, &ConnectionMgr::connUpdateDetails);

	QMutexLocker lock(&mMtxConnections);
	mConnections.push_back(aConnection);
}





void ConnectionMgr::updateDetectedDevice(DetectedDevices & aDetection, Connection & aConnection)
{
	const auto & id = aConnection.connectionID();
	aDetection.setDeviceStatus(id, deviceStatusFromConnection(aConnection));  // Also creates the device if not already exist
	const auto & friendlyName = aConnection.friendlyName();
	if (friendlyName.isPresent())
	{
		aDetection.setDeviceName(id, friendlyName.value());
	}
	const auto & avatar = aConnection.avatar();
	if (avatar.isPresent())
	{
		aDetection.setDeviceAvatar(id, avatar.value());
	}
}





DetectedDevices::Device::Status ConnectionMgr::deviceStatusFromConnection(const Connection & aConnection)
{
	if (!aConnection.remotePublicKeyData().isPresent())
	{
		return DetectedDevices::Device::dsNoPubKey;
	}

	switch (aConnection.state())
	{
		case Connection::csInitial:          return DetectedDevices::Device::dsNoPubKey;
		case Connection::csUnknownPairing:   return DetectedDevices::Device::dsNeedPairing;
		case Connection::csKnownPairing:     return DetectedDevices::Device::dsNeedPairing;
		case Connection::csRequestedPairing: return DetectedDevices::Device::dsNeedPairing;
		case Connection::csBlacklisted:      return DetectedDevices::Device::dsBlacklisted;
		case Connection::csDifferentKey:     return DetectedDevices::Device::dsNeedPairing;
		case Connection::csEncrypted:        return DetectedDevices::Device::dsOnline;
	}
	return DetectedDevices::Device::dsNeedPairing;
}





void ConnectionMgr::removeDetection(QObject * aDetection)
{
	QMutexLocker lock(&mMtxDetections);
	for (auto itr = mDetections.begin(); itr != mDetections.end();)
	{
		auto d = itr->lock().get();
		if ((d == aDetection) || (d == nullptr))
		{
			itr = mDetections.erase(itr);
		}
		else
		{
			++itr;
		}
	}
}





void ConnectionMgr::connUpdateDetails(Connection * aConnection)
{
	assert(aConnection != nullptr);

	// Update the details in all detections:
	QMutexLocker lock(&mMtxDetections);
	for (auto & det: mDetections)
	{
		auto d = det.lock();
		if (d == nullptr)
		{
			continue;
		}
		updateDetectedDevice(*d, *aConnection);
	}
}
