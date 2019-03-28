#include "TcpListener.hpp"
#include <cassert>
#include <QTcpSocket>
#include "Connection.hpp"
#include "DetectedDevices.hpp"
#include "UdpBroadcaster.hpp"
#include "PhaseHandlers.hpp"





TcpListener::TcpListener(ComponentCollection & aComponents, QObject * aParent):
	Super(aParent),
	mComponents(aComponents)
{
	connect(&mServer, &QTcpServer::newConnection, this, &TcpListener::newConnection);
}





void TcpListener::start()
{
	assert(!mServer.isListening());  // Not started yet
	mServer.listen();
}





quint16 TcpListener::listeningPort()
{
	assert(mServer.isListening());
	if (!mServer.isListening())
	{
		throw NotListeningError("The TCP server is not listening");
	}
	return mServer.serverPort();
}





std::vector<std::shared_ptr<Connection> > TcpListener::connections() const
{
	QMutexLocker lock(&mMtxConnections);
	return mConnections;
}





std::shared_ptr<DetectedDevices> TcpListener::detectDevices()
{
	auto dd = std::make_shared<DetectedDevices>();
	{
		QMutexLocker lock(&mMtxDetections);
		mDetections.push_back(dd);
	}
	connect(dd.get(), &DetectedDevices::destroyed, this, &TcpListener::removeDetection);

	// Report all currently connected devices:
	{
		QMutexLocker lock(&mMtxConnections);
		for (const auto & conn: mConnections)
		{
			updateDetectedDevice(*dd, *conn);
		}
	}
	mComponents.get<UdpBroadcaster>()->startDiscovery();
	return dd;
}





void TcpListener::newConnection()
{
	auto tcpConn = mServer.nextPendingConnection();
	if (tcpConn == nullptr)
	{
		return;
	}
	auto conn = std::make_shared<Connection>(
		mComponents,
		reinterpret_cast<QIODevice *>(tcpConn),
		Connection::tkTcp,
		tr("TCP")
	);
	// connect(tcpConn, &QTcpSocket::disconnected, conn.get(), &Connection::transportDisconnected);

	// Register the change notifications for the device detection:
	connect(conn.get(), &Connection::receivedPublicID,     this, &TcpListener::connUpdateDetails);
	connect(conn.get(), &Connection::receivedFriendlyName, this, &TcpListener::connUpdateDetails);
	connect(conn.get(), &Connection::receivedAvatar,       this, &TcpListener::connUpdateDetails);

	QMutexLocker lock(&mMtxConnections);
	mConnections.push_back(conn);
}





void TcpListener::removeDetection(QObject * aDetection)
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
	if (mDetections.empty())
	{
		mComponents.get<UdpBroadcaster>()->endDiscovery();
	}
}





void TcpListener::connUpdateDetails()
{
	auto conn = qobject_cast<Connection *>(sender());
	if (conn == nullptr)
	{
		return;
	}
	if (!conn->remotePublicID().isPresent())
	{
		// Connection hasn't provided the PublicID yet, so it's not in the detections.
		return;
	}
	QMutexLocker lock(&mMtxDetections);
	for (auto & det: mDetections)
	{
		auto d = det.lock();
		if (d == nullptr)
		{
			continue;
		}
		updateDetectedDevice(*d, *conn);
	}
}





void TcpListener::updateDetectedDevice(DetectedDevices & aDetection, Connection & aConnection)
{
	if (!aConnection.remotePublicID().isPresent())
	{
		return;
	}
	const auto & id = aConnection.remotePublicID().value();
	DetectedDevices::Device::Status status = DetectedDevices::Device::dsFirstTime;
	if (aConnection.remotePublicKeyData().isPresent())
	{
		status = DetectedDevices::Device::dsUnauthorized;
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
