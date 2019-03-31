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





void ConnectionMgr::addConnection(std::shared_ptr<Connection> aConnection)
{
	// Register the change notifications for the device detection:
	connect(aConnection.get(), &Connection::receivedPublicID,     this, &ConnectionMgr::connUpdateDetails);
	connect(aConnection.get(), &Connection::receivedFriendlyName, this, &ConnectionMgr::connUpdateDetails);
	connect(aConnection.get(), &Connection::receivedAvatar,       this, &ConnectionMgr::connUpdateDetails);

	QMutexLocker lock(&mMtxConnections);
	mConnections.push_back(aConnection);
}





void ConnectionMgr::updateDetectedDevice(DetectedDevices & aDetection, Connection & aConnection)
{
	if (!aConnection.remotePublicID().isPresent())
	{
		return;
	}
	const auto & id = aConnection.remotePublicID().value();
	DetectedDevices::Device::Status status = DetectedDevices::Device::dsNoPubKey;
	if (aConnection.remotePublicKeyData().isPresent())
	{
		status = DetectedDevices::Device::dsNeedPairing;
		// TODO: More statuses
	}
	aDetection.setDeviceStatus(id, status);  // Also creates the device if not already exist
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

	if (!aConnection->remotePublicID().isPresent())
	{
		// Connection hasn't provided the PublicID yet, so it's not in the detections.
		return;
	}

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
