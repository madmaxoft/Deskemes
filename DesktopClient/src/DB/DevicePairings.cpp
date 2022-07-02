#include "DevicePairings.hpp"
#include <cassert>
#include <QDebug>
#include <QSqlError>
#include <QSqlRecord>
#include "RsaPrivateKey.h"
#include "../ComponentCollection.hpp"
#include "Database.hpp"





DevicePairings::DevicePairings(ComponentCollection & aComponents):
	ComponentSuper(aComponents),
	mLogger(aComponents.logger("DevicePairings"))
{
	requireForStart(ComponentCollection::ckDatabase);
}





void DevicePairings::start()
{
	mLogger.log("Starting...");
	auto db = mComponents.get<Database>();
	auto conn = db->connection();
	auto query = conn.query("SELECT * FROM DevicePairings");
	if (!query.exec())
	{
		mLogger.log("ERROR: Cannot exec start statement: %1.", query.lastError());
		assert(!"DB error");
		return;
	}
	const auto & rec = query.record();
	static const QString fieldNames[] =
	{
		"DeviceID",
		"DevicePublicKeyData",
		"LocalPublicKeyData",
	};
	for (const auto & fieldName: fieldNames)
	{
		if (rec.indexOf(fieldName) == -1)
		{
			throw RuntimeError(mLogger, "DevicePairings database is broken, missing field %1.", fieldName);
		}
	}
}





Optional<DevicePairings::Pairing> DevicePairings::lookupDevice(const QByteArray & aDevicePublicID)
{
	mLogger.log("Looking up device %1...", aDevicePublicID);
	auto db = mComponents.get<Database>();
	auto conn = db->connection();
	auto query = conn.query("SELECT * FROM DevicePairings WHERE DeviceID = ?");
	query.addBindValue(aDevicePublicID);
	if (!query.exec())
	{
		mLogger.log("ERROR: Cannot exec lookup statement: %1.", query.lastError());
		assert(!"DB error");
		return {};
	}
	if (!query.next())
	{
		// No such DeviceID in the DB
		mLogger.log("Device %1 is not paired.", aDevicePublicID);
		return {};
	}
	mLogger.log("Device %1 found, returning data.", aDevicePublicID);
	return Pairing
	{
		query.value("DeviceID").toByteArray(),
		query.value("DevicePublicKeyData").toByteArray(),
		query.value("LocalPublicKeyData").toByteArray(),
		query.value("LocalPrivateKeyData").toByteArray(),
	};
}





void DevicePairings::pairDevice(
	const QString & aFriendlyName,
	const QByteArray & aDevicePublicID,
	const QByteArray & aDevicePublicKeyData,
	const QByteArray & aLocalPublicKeyData,
	const QByteArray & aLocalPrivateKeyData
)
{
	// Remove any old pairing from the DB:
	auto db = mComponents.get<Database>();
	auto conn = db->connection();
	{
		auto query = conn.query("DELETE FROM DevicePairings WHERE DeviceID = ?");
		query.addBindValue(aDevicePublicID);
		query.exec();
	}

	// Add the new pairing to the DB:
	{
		auto query = conn.query(
			"INSERT INTO DevicePairings "
			"(FriendlyName, DeviceID, DevicePublicKeyData, LocalPublicKeyData, LocalPrivateKeyData) "
			"VALUES (?, ?, ?, ?, ?)"
		);
		query.addBindValue(aFriendlyName);
		query.addBindValue(aDevicePublicID);
		query.addBindValue(aDevicePublicKeyData);
		query.addBindValue(aLocalPublicKeyData);
		query.addBindValue(aLocalPrivateKeyData);
		query.exec();
	}
	mLogger.log("Added new device pairing for device \"%1\":", aFriendlyName);
	mLogger.logHex(aDevicePublicID, "DevicePublicID");
	mLogger.logHex(aDevicePublicKeyData, "DevicePublicKeyData");
	mLogger.logHex(aLocalPublicKeyData, "LocalPublicKeyData");
	mLogger.logHex(aLocalPrivateKeyData, "LocalPrivateKeyData");  // TODO: Should we log this potentially sensitive information?
}





void DevicePairings::createLocalKeyPair(const QByteArray & aDevicePublicID, const QString & aFriendlyName)
{
	auto oldPairing = lookupDevice(aDevicePublicID);
	if (oldPairing.isPresent())
	{
		mLogger.log("Device \"%1\" already has a keypair. Ignoring creation request.", aFriendlyName);
		return;
	}

	// Generate a new keypair
	mLogger.log("Generating keypair for device \"%1\"...", aFriendlyName);
	RsaPrivateKey rpk;
	#ifdef _DEBUG
		// Generating is very slow in debug builds, use shorter keys
		rpk.generate(2048);
	#else
		rpk.generate(4096);
	#endif
	auto pubKeyData  = QByteArray::fromStdString(rpk.getPubKeyDER());
	auto privKeyData = QByteArray::fromStdString(rpk.getPrivKeyDER());
	mLogger.log("Keypair for device \"%1\" generated.", aFriendlyName);

	// Save the new keypair to the DB
	auto db = mComponents.get<Database>();
	auto conn = db->connection();
	auto query = conn.query("INSERT INTO DevicePairings (FriendlyName, DeviceID, LocalPublicKeyData, LocalPrivateKeyData) VALUES (?, ?, ?, ?)");
	query.addBindValue(aFriendlyName);
	query.addBindValue(aDevicePublicID);
	query.addBindValue(pubKeyData);
	query.addBindValue(privKeyData);
	if (!query.exec())
	{
		mLogger.log("ERROR: Cannot store generated keypair for device \"%1\" to DB, statement failed: %2", aFriendlyName, query.lastError());
		assert(!"DB error");
	}
}
