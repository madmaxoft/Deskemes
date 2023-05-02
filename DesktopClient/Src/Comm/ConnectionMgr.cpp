#include "ConnectionMgr.hpp"
#include <cassert>
#include "../DeviceMgr.hpp"





ConnectionMgr::ConnectionMgr(ComponentCollection & aComponents):
	ComponentSuper(aComponents),
	mLogger(aComponents.logger("ConnectionMgr"))
{
	requireForStart(ComponentCollection::ckDatabase);
}





ConnectionMgr::~ConnectionMgr()
{
	mLogger.log("Removing all connections...");
	{
		QMutexLocker lock(&mMtxConnections);
		for (auto & conn: mConnections)
		{
			conn->disconnect(this);
		}
		mConnections.clear();
	}
	mLogger.log("Connections removed.");
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
	mLogger.log("A non-existent connection ID was requested: %1", aConnectionID);
	return nullptr;
}





void ConnectionMgr::stop()
{
	mLogger.log("Terminating all connections...");
	QMutexLocker lock(&mMtxConnections);
	for (auto & conn: mConnections)
	{
		conn->terminate();
	}
	mLogger.log("Connections terminated...");
}





void ConnectionMgr::addConnection(std::shared_ptr<Connection> aConnection)
{
	mLogger.log("Adding a new connection, id = %1.", aConnection->connectionID());
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
	mLogger.log("Connection established, id = %1.", aConnection->connectionID());
	assert(aConnection->state() == Connection::csEncrypted);

	connect(aConnection, &Connection::disconnected, aConnection,
		[this](Connection * aCBConnection)
		{
			emit lostConnection(aCBConnection->shared_from_this());
		}
	);
	emit newConnection(aConnection->shared_from_this());
}
