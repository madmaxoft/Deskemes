#pragma once





#include "../ComponentCollection.hpp"





/** Manages the device blacklist.
Uses the Database as the data storage. */
class DeviceBlacklist:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckDeviceBlacklist>
{
	using ComponentSuper = ComponentCollection::Component<ComponentCollection::ckDeviceBlacklist>;

	Q_OBJECT


protected:

	/** The logger used for all messages produced by this class. */
	Logger & mLogger;


public:

	DeviceBlacklist(ComponentCollection & aComponents);

	/** Checks that the DB is in proper format.
	If the DB is unusable, throws a descriptive RuntimeError. */
	virtual void start() override;

	/** Returns true if the specified device is blacklisted. */
	bool isBlacklisted(const QByteArray & aDeviceID);
};
