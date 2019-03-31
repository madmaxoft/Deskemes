#include "DevicePairings.hpp"
#include <cassert>
#include <QDebug>
#include <QSqlError>
#include <QSqlRecord>
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
	};
}
