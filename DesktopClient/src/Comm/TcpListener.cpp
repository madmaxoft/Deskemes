#include "TcpListener.hpp"
#include <cassert>
#include "Connection.hpp"





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

	QMutexLocker lock(&mMtxConnections);
	mConnections.push_back(conn);
}
