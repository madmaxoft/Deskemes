#pragma once





#include <memory>
#include <vector>
#include <QObject>
#include <QMutex>
#include "../ComponentCollection.hpp"
#include "Connection.hpp"
#include "DetectedDevices.hpp"





/** Manages all connections to the devices that have been established.
Connections are uniquely identified by their ConnectionID.
Receives connections from the TCP, USB and Bluetooth listeners, manages all of them.
Also supports detection, handles even connections that haven't been paired yet. */
class ConnectionMgr:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckConnectionMgr>
{
	using ComponentSuper = ComponentCollection::Component<ComponentCollection::ckConnectionMgr>;

	Q_OBJECT


public:


	explicit ConnectionMgr(ComponentCollection & aComponents);

	virtual ~ConnectionMgr() override;

	/** Returns a (shallow) copy of all the connections. */
	std::vector<std::shared_ptr<Connection>> connections() const;

	/** Adds the specified connection to the internal list of managed connections.
	Sends the connection to all current detections.
	To be called by the TCP, USB or Bluetooth listeners when they receive a new connection. */
	void addConnection(ConnectionPtr aConnection);

	/** Returns the connection identified by the specified ConnectionID.
	Returns nullptr if there's no such connection. */
	ConnectionPtr connectionFromID(const QByteArray & aConnectionID);

	/** Disconnects all connections. */
	void stop();


protected:

	/** All current connections.
	Protected against multithreaded access by mMtxConnections. */
	std::vector<std::shared_ptr<Connection>> mConnections;

	/** The mutex protecting mConnections against multithreaded access. */
	mutable QMutex mMtxConnections;


	/** Returns the detected device status that best describes the connection's status. */
	DetectedDevices::Device::Status deviceStatusFromConnection(const Connection & aConnection);


signals:

	/** Emitted when a connection is fully established (csEncrypted).
	Used by DeviceMgr to create / update devices. */
	void newConnection(ConnectionPtr aConnection);

	/** Emitted when a previously fully established connection goes offline. */
	void lostConnection(ConnectionPtr aConnection);


protected slots:

	/** Updates the connection's details and emits the newConnection() signal. */
	void connEstablished(Connection * aConnection);
};

