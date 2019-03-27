#include "TcpListener.hpp"
#include <cassert>
#include "Connection.hpp"





TcpListener::TcpListener(ComponentCollection & aComponents, QObject * aParent):
	Super(aParent),
	mComponents(aComponents)
{
	mThread.setObjectName("TcpListener");
	mThread.start();
	// TODO: Finish threading. Right now it doesn't work with mServer.listen() called in another thread.
	// mServer.moveToThread(&mThread);
	connect(&mServer, &QTcpServer::newConnection, this, &TcpListener::newConnection);
}





TcpListener::~TcpListener()
{
	mServer.close();
	mThread.quit();
	if (!mThread.wait(1000))
	{
		qWarning() << "Failed to stop the TcpListener thread, aborting forcefully.";
	}
}





void TcpListener::start()
{
	assert(!mServer.isListening());  // Not started yet
	// TODO: Finish threading. Right now it doesn't work with mServer.listen() called in another thread.
	// QTcpServer.listen() is not invokable
	// QMetaObject::invokeMethod(&mServer, "listen", Qt::BlockingQueuedConnection);
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
	mConnections.push_back(conn);
}
