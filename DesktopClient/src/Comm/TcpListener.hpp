#pragma once

#include <memory>
#include <QThread>
#include <QTcpServer>
#include <QMutex>
#include "../ComponentCollection.hpp"





// fwd:
class Connection;
class DetectedDevices;





/** Listens for the TCP connections from the device app. */
class TcpListener:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckTcpListener>
{
	using Super = QObject;
	using ComponentSuper = ComponentCollection::Component<ComponentCollection::ckTcpListener>;

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
	virtual void start() override;

	/** Stops listening. */
	void stop();

	/** The port on which mServer is listening.
	Asserts and throws a NotListeningError if not started yet. */
	quint16 listeningPort();


protected:

	/** The TCP server used for listening for connections. */
	QTcpServer mServer;



protected Q_SLOTS:

	/** Creates a new Connection object for the new connection from the TCP server.
	Emitted by mServer when a new connection is requested. */
	void newConnection();
};
