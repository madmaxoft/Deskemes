#pragma once

#include <memory>
#include <string>
#include <QObject>





/** Encapsulates data for a single device. */
class Device:
	public QObject
{
	using Super = QObject;
	Q_OBJECT

public:

	explicit Device(const std::string & aDeviceID);

	// Simple getters:
	const std::string & deviceID() const { return mDeviceID; }


protected:

	/** The identifier that uniquely identifies the device. */
	std::string mDeviceID;


signals:

public slots:
};

using DevicePtr = std::shared_ptr<Device>;

Q_DECLARE_METATYPE(DevicePtr);
