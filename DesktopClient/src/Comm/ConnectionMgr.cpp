#include "ConnectionMgr.hpp"
#include <cassert>
#include "../DeviceMgr.hpp"





ConnectionMgr::ConnectionMgr(ComponentCollection & aComponents):
	ComponentSuper(aComponents)
{
	requireForStart(ComponentCollection::ckDatabase);
}





ConnectionMgr::~ConnectionMgr()
{
	{
		QMutexLocker lock(&mMtxConnections);
		for (auto & conn: mConnections)
		{
			conn->disconnect(this);
		}
		mConnections.clear();
	}
}





std::vector<std::shared_ptr<Connection>> ConnectionMgr::connections() const
{
	QMutexLocker lock(&mMtxConnections);
	return mConnections;
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





void ConnectionMgr::stop()
{
	QMutexLocker lock(&mMtxConnections);
	for (auto & conn: mConnections)
	{
		conn->terminate();
	}
}





void ConnectionMgr::addConnection(std::shared_ptr<Connection> aConnection)
{
	connect(aConnection.get(), &Connection::established, this, &ConnectionMgr::connEstablished);

	QMutexLocker lock(&mMtxConnections);
	mConnections.push_back(aConnection);
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
		case Connection::csDisconnected:     return DetectedDevices::Device::dsOffline;
	}
	return DetectedDevices::Device::dsNeedPairing;
}





void ConnectionMgr::connEstablished(Connection * aConnection)
{
	assert(aConnection->state() == Connection::csEncrypted);

	connect(aConnection, &Connection::disconnected, aConnection,
		[this](Connection * aCBConnection)
		{
			emit lostConnection(aCBConnection->shared_from_this());
		}
	);
	emit newConnection(aConnection->shared_from_this());
}
