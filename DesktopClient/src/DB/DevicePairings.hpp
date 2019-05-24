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

		/** The local private key that we've used to pair with the device. */
		QByteArray mLocalPrivateKeyData;
	};


	explicit DevicePairings(ComponentCollection & aComponents);

	/** Checks that the DB is in proper format.
	If the DB is unusable, throws a descriptive RuntimeError. */
	void start();

	/** Looks up the specified device, and returns its pairing data, if available. */
	Optional<Pairing> lookupDevice(const QByteArray & aDevicePublicID);

	/** Adds / modifies a pairing for the specified device. */
	void pairDevice(
		const QString & aFriendlyName,
		const QByteArray & aDevicePublicID,
		const QByteArray & aDevicePublicKeyData,
		const QByteArray & aLocalPublicKeyData,
		const QByteArray & aLocalPrivateKeyData
	);

	/** Generates a new keypair for the specified device and stores it in the DB.
	Silently ignored if a keypair for the device already exists. */
	void createLocalKeyPair(const QByteArray & aDevicePublicID, const QString & aFriendlyName);


protected:

	/** The components of the entire app. */
	ComponentCollection & mComponents;
};
