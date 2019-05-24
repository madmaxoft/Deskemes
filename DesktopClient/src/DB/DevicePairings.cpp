#include "DevicePairings.hpp"
#include <cassert>
#include <QDebug>
#include <QSqlError>
#include <QSqlRecord>
#include "RsaPrivateKey.h"
#include "../ComponentCollection.hpp"
#include "Database.hpp"





DevicePairings::DevicePairings(ComponentCollection & aComponents):
	mComponents(aComponents)
{
}





void DevicePairings::start()
{
	auto db = mComponents.get<Database>();
	auto conn = db->connection();
	auto query = conn.query("SELECT * FROM DevicePairings");
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
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
			qWarning() << "DevicePairings database is broken, missing field " << fieldName;
			throw RuntimeError("DevicePairings database is broken, missing field %1.", fieldName);
		}
	}
}





Optional<DevicePairings::Pairing> DevicePairings::lookupDevice(const QByteArray & aDevicePublicID)
{
	auto db = mComponents.get<Database>();
	auto conn = db->connection();
	auto query = conn.query("SELECT * FROM DevicePairings WHERE DeviceID = ?");
	query.addBindValue(aDevicePublicID);
	if (!query.exec())
	{
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return {};
	}
	if (!query.next())
	{
		// No such DeviceID in the DB
		return {};
	}
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
}





void DevicePairings::createLocalKeyPair(const QByteArray & aDevicePublicID, const QString & aFriendlyName)
{
	auto oldPairing = lookupDevice(aDevicePublicID);
	if (oldPairing.isPresent())
	{
		qDebug() << "Device " << aFriendlyName << " already has a keypair. Ignoring creation request.";
		return;
	}

	// Generate a new keypair
	qDebug() << "Generating keypair...";
	RsaPrivateKey rpk;
	#ifdef _DEBUG
		// Generating is very slow in debug builds, use shorter keys
		rpk.generate(1024);
	#else
		rpk.generate(4096);
	#endif
	auto pubKeyData  = QByteArray::fromStdString(rpk.getPubKeyDER());
	auto privKeyData = QByteArray::fromStdString(rpk.getPrivKeyDER());
	qDebug() << "Keypair generated.";

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
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
	}
}
