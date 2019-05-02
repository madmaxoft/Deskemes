#include "TcpListener.hpp"
#include <cassert>
#include <QTcpSocket>
#include "Connection.hpp"
#include "DetectedDevices.hpp"
#include "UdpBroadcaster.hpp"
#include "ConnectionMgr.hpp"





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





void TcpListener::stop()
{
	assert(mServer.isListening());
	mServer.close();
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





void TcpListener::newConnection()
{
	auto tcpConn = mServer.nextPendingConnection();
	if (tcpConn == nullptr)
	{
		return;
	}
	auto id = QString("TCP:[%1]:%2").arg(tcpConn->peerAddress().toString()).arg(tcpConn->peerPort()).toUtf8();
	auto conn = std::make_shared<Connection>(
		mComponents,
		id,
		reinterpret_cast<QIODevice *>(tcpConn),
		Connection::tkTcp,
		tcpConn->peerAddress().toString()
	);
	mComponents.get<ConnectionMgr>()->addConnection(conn);
}
