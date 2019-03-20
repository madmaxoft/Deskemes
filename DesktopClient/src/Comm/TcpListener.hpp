#pragma once

#include <QThread>
#include <QTcpServer>
#include "../ComponentCollection.hpp"





/** Listens for the UDP beacons from the device app. */
class TcpListener:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckTcpListener>
{
	using Super = QObject;
	Q_OBJECT


public:

	/** The exception that is thrown when the server is not listening and its port is queried. */
	class NotListeningError:
		public RuntimeError
	{
	public:
		using RuntimeError::RuntimeError;
	};


	explicit TcpListener(QObject * aParent = nullptr);

	virtual ~TcpListener() override;

	/** Starts listening on a system-assigned TCP port on all interfaces. */
	void start();

	/** The port on which mServer is listening.
	Asserts and throws a NotListeningError if not started yet. */
	quint16 listeningPort();


protected:

	/** The thread on which the UDP socket lives. */
	QThread mThread;

	/** The TCP server used for listening for connections. */
	QTcpServer mServer;



signals:


public slots:


protected slots:

	/** Creates a new Connection object for the new connection from the TCP server.
	Emitted by mServer when a new connection is requested. */
	void newConnection();
};