#include "DatabaseUpgrade.hpp"
#include <vector>
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>
#include "Database.hpp"





class VersionScript
{
public:
	VersionScript(std::vector<std::string> && aCommands):
		mCommands(std::move(aCommands))
	{}

	std::vector<std::string> mCommands;


	/** Applies this upgrade script to the specified DB, and updates its version to a_Version. */
	void apply(QSqlDatabase & aDB, size_t aVersion) const
	{
		qDebug() << "Executing DB upgrade script to version " << aVersion;

		// Temporarily disable FKs:
		{
			auto query = aDB.exec("pragma foreign_keys = off");
			if (query.lastError().type() != QSqlError::NoError)
			{
				qWarning() << "SQL query failed: " << query.lastError();
				throw DatabaseUpgrade::SqlError(query.lastError(), query.lastQuery().toStdString());
			}
		}

		// Begin transaction:
		{
			auto query = aDB.exec("begin");
			if (query.lastError().type() != QSqlError::NoError)
			{
				qWarning() << "SQL query failed: " << query.lastError();
				throw DatabaseUpgrade::SqlError(query.lastError(), query.lastQuery().toStdString());
			}
		}

		// Execute the individual commands:
		for (const auto & cmd: mCommands)
		{
			auto query = aDB.exec(QString::fromStdString(cmd));
			if (query.lastError().type() != QSqlError::NoError)
			{
				qWarning() << "SQL upgrade command failed: " << query.lastError();
				qDebug() << "  ^-- command: " << cmd.c_str();
				throw DatabaseUpgrade::SqlError(query.lastError(), cmd);
			}
		}

		// Set the new version:
		{
			auto query = aDB.exec(QString("UPDATE Version SET Version = %1").arg(aVersion));
			if (query.lastError().type() != QSqlError::NoError)
			{
				qWarning() << "SQL transaction commit failed: " << query.lastError();
				throw DatabaseUpgrade::SqlError(query.lastError(), query.lastQuery().toStdString());
			}
		}

		// Check the FK constraints:
		{
			auto query = aDB.exec("pragma check_foreign_keys");
			if (query.lastError().type() != QSqlError::NoError)
			{
				qWarning() << "SQL transaction commit failed: " << query.lastError();
				throw DatabaseUpgrade::SqlError(query.lastError(), query.lastQuery().toStdString());
			}
		}

		// Commit the transaction:
		{
			auto query = aDB.exec("commit");
			if (query.lastError().type() != QSqlError::NoError)
			{
				qWarning() << "SQL transaction commit failed: " << query.lastError();
				throw DatabaseUpgrade::SqlError(query.lastError(), query.lastQuery().toStdString());
			}
		}

		// Re-enable FKs:
		{
			auto query = aDB.exec("pragma foreign_keys = on");
			if (query.lastError().type() != QSqlError::NoError)
			{
				qWarning() << "SQL query failed: " << query.lastError();
				throw DatabaseUpgrade::SqlError(query.lastError(), query.lastQuery().toStdString());
			}
		}
	}
};






static const std::vector<VersionScript> g_VersionScripts =
{
	// Version 0 (empty DB) to version 1:
	VersionScript({
		"CREATE TABLE IF NOT EXISTS Version ("
			"Version INTEGER"
		")",

		"INSERT INTO Version (Version) VALUES (1)",
	}),  // Version 0 to Version 1



	// Version 1 to Version 2:
	// Added DevicePairings and DeviceBlacklist
	VersionScript({
		"CREATE TABLE DevicePairings ("
			"DeviceID            BLOB,"
			"DevicePublicKeyData BLOB,"
			"LocalPublicKeyData  BLOB"
		")",

		"CREATE TABLE DeviceBlacklist ("
			"DeviceID BLOB"
		")",
	}),  // Version 1 to Version 2



	// Version 2 to Version 3:
	// Added private key data to DevicePairings
	VersionScript({
		"ALTER TABLE DevicePairings ADD COLUMN LocalPrivateKeyData BLOB",
	}),  // Version 2 to Version 3

	// Version 3 to Version 4:
	// Added FriendlyName to DevicePairings
	VersionScript({
		"ALTER TABLE DevicePairings ADD COLUMN FriendlyName",
	}),  // Version 2 to Version 3
};





////////////////////////////////////////////////////////////////////////////////
// DatabaseUpgrade:

DatabaseUpgrade::DatabaseUpgrade(Database & aDB):
	mDB(aDB.database())
{
}





void DatabaseUpgrade::upgrade(Database & aDB)
{
	DatabaseUpgrade upg(aDB);
	return upg.execute();
}





size_t DatabaseUpgrade::currentVersion()
{
	return g_VersionScripts.size();
}





void DatabaseUpgrade::execute()
{
	auto version = getVersion();
	qDebug() << "DB is at version " << version;
	bool hasUpgraded = false;
	for (auto i = version; i < g_VersionScripts.size(); ++i)
	{
		qWarning() << "Upgrading DB to version" << i + 1;
		g_VersionScripts[i].apply(mDB, i + 1);
		hasUpgraded = true;
	}

	// After upgrading, vacuum the leftover space:
	if (hasUpgraded)
	{
		auto query = mDB.exec("VACUUM");
		if (query.lastError().type() != QSqlError::NoError)
		{
			throw SqlError(query.lastError(), "VACUUM");
		}
	}
}





size_t DatabaseUpgrade::getVersion()
{
	auto query = mDB.exec("SELECT MAX(Version) AS Version FROM Version");
	if (!query.first())
	{
		return 0;
	}
	return query.record().value("Version").toULongLong();
}





////////////////////////////////////////////////////////////////////////////////
// DatabaseUpgrade::SqlError:

DatabaseUpgrade::SqlError::SqlError(const QSqlError & aSqlError, const std::string & aSqlCommand):
	Super("Failed to upgrade database: %1 (command \"%2\")", aSqlError, aSqlCommand)
{
}
