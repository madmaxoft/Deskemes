#pragma once





#include <QObject>
#include "../Optional.hpp"
#include "../ComponentCollection.hpp"





/** Manages the list of paired devices and their public keys.
Uses the Database as the data storage. */
class DevicePairings:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckDevicePairings>
{
	using Super = QObject;
	Q_OBJECT

public:

	/** Container for data of a single pairing. */
	struct Pairing
	{
		/** The Public ID of the device. */
		QByteArray mDevicePublicID;

		/** The public key of the device, as raw data. */
		QByteArray mDevicePublicKeyData;

		/** The local public key that we've used to pair with the device. */
		QByteArray mLocalPublicKeyData;
	};


	explicit DevicePairings(ComponentCollection & aComponents);

	/** Checks that the DB is in proper format.
	If the DB is unusable, throws a descriptive RuntimeError. */
	void start();

	/** Looks up the specified device, and returns its pairing data, if available. */
	Optional<Pairing> lookupDevice(const QByteArray & aDevicePublicID);

	/** Adds / modifies a pairing for the specified device. */
	void pairDevice(
		const QByteArray & aDevicePublicID,
		const QByteArray & aDevicePublicKeyData,
		const QByteArray & aLocalPublicKeyData
	);


protected:

	/** The components of the entire app. */
	ComponentCollection & mComponents;
};
