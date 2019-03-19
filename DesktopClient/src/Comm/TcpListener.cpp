#include "TcpListener.hpp"
#include <cassert>





TcpListener::TcpListener(QObject * aParent):
	Super(aParent)
{
	mThread.setObjectName("TcpListener");
	mThread.start();
	mServer.moveToThread(&mThread);
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
	// TODO
}
