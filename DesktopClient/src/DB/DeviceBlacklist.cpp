#include "DeviceBlacklist.hpp"
#include <cassert>
#include <QSqlError>
#include <QSqlRecord>
#include "Database.hpp"





DeviceBlacklist::DeviceBlacklist(ComponentCollection & aComponents):
	mComponents(aComponents)
{
}





void DeviceBlacklist::start()
{
	auto db = mComponents.get<Database>();
	auto conn = db->connection();
	auto query = conn.query("SELECT * FROM DeviceBlacklist");
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
	};
	for (const auto & fieldName: fieldNames)
	{
		if (rec.indexOf(fieldName) == -1)
		{
			qWarning() << "DeviceBlacklist database is broken, missing field " << fieldName;
			throw RuntimeError("DeviceBlacklist database is broken, missing field %1.", fieldName);
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
		qWarning() << "Cannot exec statement: " << query.lastError();
		assert(!"DB error");
		return {};
	}
	if (!query.next())
	{
		// No such DeviceID in the blacklist
		return false;
	}
	return true;
}
