#include "TcpListener.hpp"
#include <cassert>
#include <QTcpSocket>
#include <QDebug>
#include "Connection.hpp"
#include "DetectedDevices.hpp"
#include "UdpBroadcaster.hpp"
#include "ConnectionMgr.hpp"





TcpListener::TcpListener(ComponentCollection & aComponents, QObject * aParent):
	Super(aParent),
	ComponentSuper(aComponents),
	mLogger(aComponents.logger("LocalNetListener"))
{
	connect(&mServer, &QTcpServer::newConnection, this, &TcpListener::newConnection);
}





void TcpListener::start()
{
	assert(!mServer.isListening());  // Not started yet
	if (!mServer.listen(QHostAddress::Any, 24816))
	{
		mLogger.log("ERROR: Failed to listen on preferred port 24816, attempting any port");
		mServer.listen();
	}
	mLogger.log("The TCP listener has started on port %1", listeningPort());
}





void TcpListener::stop()
{
	mLogger.log("Stopping the server...");
	assert(mServer.isListening());
	mServer.close();
	mLogger.log("Server stopped.");
}





quint16 TcpListener::listeningPort()
{
	assert(mServer.isListening());
	if (!mServer.isListening())
	{
		throw NotListeningError(mLogger, "The TCP server is not listening");
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
	mLogger.log("New connection: id = %1", id);
	auto conn = std::make_shared<Connection>(
		mComponents,
		id,
		reinterpret_cast<QIODevice *>(tcpConn),
		Connection::tkTcp,
		tcpConn->peerAddress().toString()
	);
	mComponents.get<ConnectionMgr>()->addConnection(conn);
}
