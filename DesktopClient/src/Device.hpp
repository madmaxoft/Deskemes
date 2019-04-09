#pragma once

#include <memory>
#include <QObject>
#include "Comm/Connection.hpp"





// fwd:
class Device;
using DevicePtr = std::shared_ptr<Device>;





/** Encapsulates data for a single device. */
class Device:
	public QObject,
	public std::enable_shared_from_this<Device>
{
	using Super = QObject;
	Q_OBJECT

public:

	/** Creates a device that has the specified connection. */
	explicit Device(ConnectionPtr aConnection);

	// Simple getters:
	const QByteArray & deviceID() const { return mDeviceID; }

	std::vector<ConnectionPtr> connections() const { return mConnections; }

	/** Adds the specified connection to the list of connections, and starts monitoring it for activity. */
	void addConnection(ConnectionPtr aConnection);

	/** Returns the friendly name received from the device.
	Returns an empty string if there are no connections. */
	const QString & friendlyName() const;


protected:

	/** The identifier that uniquely identifies the device (Connection::mRemotePublicID). */
	QByteArray mDeviceID;

	/** The connections through which the device is currently connected. */
	std::vector<ConnectionPtr> mConnections;


signals:

	void connectionAdded(DevicePtr aSelf, ConnectionPtr aConnection);


public slots:
};

Q_DECLARE_METATYPE(DevicePtr);
