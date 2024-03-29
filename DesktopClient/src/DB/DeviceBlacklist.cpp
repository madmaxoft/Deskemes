#include "DeviceBlacklist.hpp"
#include <cassert>
#include <QSqlError>
#include <QSqlRecord>
#include "Database.hpp"





DeviceBlacklist::DeviceBlacklist(ComponentCollection & aComponents):
	ComponentSuper(aComponents),
	mLogger(aComponents.logger("DeviceBlacklist"))
{
	requireForStart(ComponentCollection::ckDatabase);
}





void DeviceBlacklist::start()
{
	mLogger.log("Starting...");
	auto db = mComponents.get<Database>();
	auto conn = db->connection();
	auto query = conn.query("SELECT * FROM DeviceBlacklist");
	if (!query.exec())
	{
		mLogger.log("Cannot exec statement: %1.", query.lastError());
		assert(!"DB error");
		return;
	}
	const auto & rec = query.record();
	static const QString fieldNames[] =
	{
		"DeviceID",
	};
	for (const auto & fieldName: fieldNames)
	{
		if (rec.indexOf(fieldName) == -1)
		{
			throw RuntimeError(mLogger, "DeviceBlacklist database is broken, missing field %1.", fieldName);
		}
	}
}





bool DeviceBlacklist::isBlacklisted(const QByteArray & aDeviceID)
{
	auto db = mComponents.get<Database>();
	auto conn = db->connection();
	auto query = conn.query("SELECT * FROM DeviceBlacklist WHERE DeviceID = ?");
	query.addBindValue(aDeviceID);
	if (!query.exec())
	{
		mLogger.log("Cannot exec statement: %1.", query.lastError());
		assert(!"DB error");
		return true;
	}
	if (!query.next())
	{
		// No such DeviceID in the blacklist
		return false;
	}
	return true;
}
