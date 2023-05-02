#pragma once

#include <memory>
#include <QObject>
#include <QTimer>
#include "Comm/Connection.hpp"





// fwd:
class Device;
class InfoChannel;
using DevicePtr = std::shared_ptr<Device>;





/** Encapsulates data for a single device. */
class Device:
	public QObject,
	public std::enable_shared_from_this<Device>
{
	using Super = QObject;
	Q_OBJECT

public:

	/** Creates a device of the specified public ID that has no connection yet. */
	explicit Device(ComponentCollection & aComponents, const QByteArray & aDeviceID);

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

	/** The channel currently used for querying information from the device. */
	std::shared_ptr<InfoChannel> mInfoChannel;

	/** The timer used for querying status information from the device periodically. */
	QTimer mInfoQueryTimer;

	/** The logger used for all messages produced by this class. */
	Logger & mLogger;


	/** Opens an InfoChannel on one of the connections, sets it to mInfoChannel. */
	void openInfoChannel();

	/** Returns a logger appropriate for a device using the specified connection. */
	static Logger & createLogger(ComponentCollection & aComponents, const QString & aDeviceID);


signals:

	/** Emitted after a new Connection is added to mConnections. */
	void connectionAdded(DevicePtr aSelf, ConnectionPtr aConnection);

	/** Emitted when identification is received through the mInfoChannel. */
	void identificationUpdated(const QString & aImei, const QString & aImsi, const QString & aCarrierName);

	/** Emitted when the battery level is received from the device. */
	void batteryUpdated(const double aBatteryLevelPercent);

	/** Emitted when the signal strength is received from the device. */
	void signalStrengthUpdated(const double aSignalStrengthPercent);

	/** Emitted when the last connection is lost and the device is going offline. */
	void goingOffline();


private slots:

	/** Queries the status information from the device, using the current mInfoChannel.
	If the info channel is invalid, attempts to open a new one.
	Called from mInfoQueryTimer. */
	void queryStatusInfo();

	/** Received the device's IMEI, IMSI or CarrierName through the mInfoChannel.
	Emits the identificationUpdated() signal with correct values (cached in mInfoChannel). */
	void receivedIdentification();

	/** The specified connection has been lost, removes it from mConnections and goes offline if it was
	the last connection to the device. */
	void connDisconnected(Connection * aConnection);
};

Q_DECLARE_METATYPE(DevicePtr);
