#pragma once

#include <QDateTime>
#include "../Connection.hpp"





/** Implements the Info channel protocol.
Provides methods to query values from the device, stores the received values and signalizes known values
through specific receivedXYZ() signals. */
class InfoChannel:
	public Connection::Channel
{
	using Super = Connection::Channel;

	Q_OBJECT


public:

	InfoChannel(Connection & aConnection);

	/** Queries all values from the device. */
	void queryAll();

	/** Queries the battery status from the device. */
	void queryBattery();

	/** Queries the signal strength from the device. */
	void querySignal();

	/** Queries the IMEI, IMSI and the carrier name from the device. */
	void queryIdentification();

	/** Queries the current datetime from the device. */
	void queryCurrentTime();

	/** Returns a deep copy of all values currently stored. */
	std::map<quint32, QVariant> allValues() const;

	/** Returns a copy of the cached value of the specified key, or an empty variant if not present. */
	QVariant value(const quint32 aKey) const;

	// These return the cached values:
	double batteryLevel() const;
	double signalStrength() const;
	QString carrierName() const;
	QString imei() const;
	QString imsi() const;
	QDateTime currentTime() const;


protected:

	/** The cache of all received values.
	Protected against multithreaded access by mMtx. */
	std::map<quint32, QVariant> mValues;

	/** Protects mValues against multithreaded access. */
	mutable QMutex mMtx;


	/** Stores the specified value, and emits the valueChanged() signal.
	For known value keys, it also triggers the appropriate specific signal (receivedXYZ()). */
	void setValue(const quint32 aKey, QVariant && aValue);

	// Channel override:
	void processIncomingMessage(const QByteArray & aMessage) override;


signals:

	/** Emitted each time when a value is received from the device. */
	void valueChanged(quint32 aKey, const QVariant & aValue);

	/** Emitted when the total battery percentage is received from the device. */
	void receivedBattery(const double aTotalPercent);

	/** Emitted when the primary signal strength value is received from the device. */
	void receivedSignalStrength(const double aSignalStrengthPercent);

	/** Emitted when the primary carrier name value is received from the device. */
	void receivedCarrierName(const QString & aCarrierName);

	/** Emitted when the primary IMEI value is received from the device. */
	void receivedImei(const QString & aImei);

	/** Emitted when the primary IMSI value is received from the device. */
	void receivedImsi(const QString & aImei);

	/** Emitted when the current date and time value is received from the device. */
	void receivedTime(const QDateTime & aCurrentTime);
};
