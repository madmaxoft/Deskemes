#include "DatabaseUpgrade.hpp"
#include <vector>
#include <QSqlError>
#include <QSqlRecord>
#include "Database.hpp"





class VersionScript
{
public:
	VersionScript(std::vector<std::string> && aCommands):
		mCommands(std::move(aCommands))
	{}

	std::vector<std::string> mCommands;


	/** Applies this upgrade script to the specified DB, and updates its version to a_Version. */
	void apply(QSqlDatabase & aDB, size_t aVersion, Logger & aLogger) const
	{
		aLogger.log("Executing DB upgrade script to version %1.", aVersion);

		// Temporarily disable FKs:
		{
			auto query = aDB.exec("pragma foreign_keys = off");
			if (query.lastError().type() != QSqlError::NoError)
			{
				aLogger.log("ERROR: Disabling FKs failed: \"%1\".", query.lastError());
				throw DatabaseUpgrade::SqlError(aLogger, query.lastError(), query.lastQuery().toStdString());
			}
		}

		// Begin transaction:
		{
			auto query = aDB.exec("begin");
			if (query.lastError().type() != QSqlError::NoError)
			{
				aLogger.log("ERROR: Starting an SQL transaction failed: \"%1\".", query.lastError());
				throw DatabaseUpgrade::SqlError(aLogger, query.lastError(), query.lastQuery().toStdString());
			}
		}

		// Execute the individual commands:
		for (const auto & cmd: mCommands)
		{
			auto query = aDB.exec(QString::fromStdString(cmd));
			if (query.lastError().type() != QSqlError::NoError)
			{
				aLogger.log("ERROR: SQL upgrade command failed. Command = \"%1\", error = \"%2\".", cmd, query.lastError());
				throw DatabaseUpgrade::SqlError(aLogger, query.lastError(), cmd);
			}
		}

		// Set the new version:
		{
			auto query = aDB.exec(QString("UPDATE Version SET Version = %1").arg(aVersion));
			if (query.lastError().type() != QSqlError::NoError)
			{
				aLogger.log("ERROR: updating DB version failed: \"%1\".", query.lastError());
				throw DatabaseUpgrade::SqlError(aLogger, query.lastError(), query.lastQuery().toStdString());
			}
		}

		// Check the FK constraints:
		{
			auto query = aDB.exec("pragma check_foreign_keys");
			if (query.lastError().type() != QSqlError::NoError)
			{
				aLogger.log("ERROR: Checking FKs failed: \"%1\".", query.lastError());
				throw DatabaseUpgrade::SqlError(aLogger, query.lastError(), query.lastQuery().toStdString());
			}
		}

		// Commit the transaction:
		{
			auto query = aDB.exec("commit");
			if (query.lastError().type() != QSqlError::NoError)
			{
				aLogger.log("ERROR: SQL transaction commit failed: \"%1\".", query.lastError());
				throw DatabaseUpgrade::SqlError(aLogger, query.lastError(), query.lastQuery().toStdString());
			}
		}

		// Re-enable FKs:
		{
			auto query = aDB.exec("pragma foreign_keys = on");
			if (query.lastError().type() != QSqlError::NoError)
			{
				aLogger.log("ERROR: Enabling FKs failed: \"%1\".", query.lastError());
				throw DatabaseUpgrade::SqlError(aLogger, query.lastError(), query.lastQuery().toStdString());
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

DatabaseUpgrade::DatabaseUpgrade(Database & aDB, Logger & aLogger):
	mDB(aDB.database()),
	mLogger(aLogger)
{
}





void DatabaseUpgrade::upgrade(Database & aDB, Logger & aLogger)
{
	DatabaseUpgrade upg(aDB, aLogger);
	return upg.execute();
}





size_t DatabaseUpgrade::currentVersion()
{
	return g_VersionScripts.size();
}





void DatabaseUpgrade::execute()
{
	auto version = getVersion();
	mLogger.log("DB is at version %1, program DB version is %2", version, g_VersionScripts.size());
	bool hasUpgraded = false;
	for (auto i = version; i < g_VersionScripts.size(); ++i)
	{
		mLogger.log("Upgrading DB to version %1", i + 1);
		g_VersionScripts[i].apply(mDB, i + 1, mLogger);
		hasUpgraded = true;
	}

	// After upgrading, vacuum the leftover space:
	if (hasUpgraded)
	{
		mLogger.log("Vacuuming the DB");
		auto query = mDB.exec("VACUUM");
		if (query.lastError().type() != QSqlError::NoError)
		{
			throw SqlError(mLogger, query.lastError(), "VACUUM");
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

DatabaseUpgrade::SqlError::SqlError(
	Logger & aLogger,
	const QSqlError & aSqlError,
	const std::string & aSqlCommand
):
	Super(aLogger, "Failed to upgrade database: %1 (command \"%2\")", aSqlError, aSqlCommand)
{
}
