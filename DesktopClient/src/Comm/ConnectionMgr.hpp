#pragma once





#include <memory>
#include <vector>
#include <QObject>
#include <QMutex>
#include "../ComponentCollection.hpp"
#include "Connection.hpp"
#include "DetectedDevices.hpp"





/** Manages all connections to the devices that have been established.
Connections are uniquely identified by their EnumeratorID.
Receives connections from the TCP, USB and Bluetooth listeners, manages all of them.
Also supports detection, handles even connections that haven't been paired yet. */
class ConnectionMgr:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckConnectionMgr>
{
	Q_OBJECT

public:


	explicit ConnectionMgr(ComponentCollection & aComponents);

	/** Returns a (shallow) copy of all the connections. */
	std::vector<std::shared_ptr<Connection>> connections() const;

	/** Starts device detection.
	Returns a new DetectedDevices instance that is the destination where the detected devices are stored.
	The detection will terminate once the returned object is destroyed. */
	std::shared_ptr<DetectedDevices> detectDevices();

	/** Adds the specified connection to the internal list of managed connections.
	Sends the connection to all current detections.
	To be called by the TCP, USB or Bluetooth listeners when they receive a new connection. */
	void addConnection(ConnectionPtr aConnection);

	/** Returns the connection identified by the specified ConnectionID.
	Returns nullptr if there's no such connection. */
	ConnectionPtr connectionFromID(const QByteArray & aConnectionID);


protected:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** All current connections.
	Protected against multithreaded access by mMtxConnections. */
	std::vector<std::shared_ptr<Connection>> mConnections;

	/** The mutex protecting mConnections against multithreaded access. */
	mutable QMutex mMtxConnections;

	/** All device detection requests that have been started by detectDevices().
	All newly found devices (connections) are reported to all items here.
	Protected against multithreaded access by mMtxDetections. */
	std::vector<std::weak_ptr<DetectedDevices>> mDetections;

	/** The mutex protecting mDetections against multithreaded access. */
	mutable QMutex mMtxDetections;


	/** Updates the details of the detected device represented by the specified connection in
	the specified detection. */
	void updateDetectedDevice(DetectedDevices & aDetection, Connection & aConnection);

	/** Returns the detected device status that best describes the connection's status. */
	DetectedDevices::Device::Status deviceStatusFromConnection(const Connection & aConnection);


signals:


public slots:


protected slots:

	/** Removes the specified detection from mDetections.
	Called by the detection itself when it is being destroyed. */
	void removeDetection(QObject * aDetection);

	/** Updates the connection's device's details in all current detections. */
	void connUpdateDetails(Connection * aConnection);
};

