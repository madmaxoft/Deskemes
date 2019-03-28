#pragma once

#include <memory>
#include <QThread>
#include <QTcpServer>
#include <QMutex>
#include "../ComponentCollection.hpp"





// fwd:
class Connection;





/** Listens for the TCP connections from the device app. */
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


	explicit TcpListener(ComponentCollection & aComponents, QObject * aParent = nullptr);

	/** Starts listening on a system-assigned TCP port on all interfaces. */
	void start();

	/** The port on which mServer is listening.
	Asserts and throws a NotListeningError if not started yet. */
	quint16 listeningPort();

	/** Returns a (shallow) copy of all the connections. */
	std::vector<std::shared_ptr<Connection>> connections() const;


protected:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The TCP server used for listening for connections. */
	QTcpServer mServer;

	/** All current TCP connections.
	Protected against multithreaded access by mMtxConnections. */
	std::vector<std::shared_ptr<Connection>> mConnections;

	/** The mutex protecting mConnections against multithreaded access. */
	mutable QMutex mMtxConnections;



signals:


public slots:


protected slots:

	/** Creates a new Connection object for the new connection from the TCP server.
	Emitted by mServer when a new connection is requested. */
	void newConnection();
};
