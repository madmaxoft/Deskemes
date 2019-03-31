#pragma once





#include "../ComponentCollection.hpp"





/** Manages the device blacklist.
Uses the Database as the data storage. */
class DeviceBlacklist:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckDeviceBlacklist>
{
	Q_OBJECT

public:

	DeviceBlacklist(ComponentCollection & aComponents);

	/** Checks that the DB is in proper format.
	If the DB is unusable, throws a descriptive RuntimeError. */
	void start();

	/** Returns true if the specified device is blacklisted. */
	bool isBlacklisted(const QByteArray & aDeviceID);


protected:

	/** The components of the entire app. */
	ComponentCollection & mComponents;
};
