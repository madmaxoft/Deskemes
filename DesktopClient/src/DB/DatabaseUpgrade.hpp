#pragma once

#include <string>
#include <QSqlError>
#include "../Exception.hpp"





// fwd:
class Database;
class QSqlDatabase;





class DatabaseUpgrade
{
public:

	/** An exception that is thrown on upgrade error if the SQL command fails. */
	class SqlError:
		public RuntimeError
	{
		using Super = RuntimeError;

	public:
		/** Creates a new instance, based on the SQL error reported for the command. */
		SqlError(Logger & aLogger, const QSqlError & aSqlError, const std::string & aSqlCommand);
	};


	/** Upgrades the database to the latest known version.
	Throws SqlError if the upgrade fails.
	Tries its best to keep the DB in a usable state, even if the upgrade fails. */
	static void upgrade(Database & aDB, Logger & aLogger);

	/** Returns the highest version that the upgrade knows (current version). */
	static size_t currentVersion();


protected:


	/** The SQL database on which to perform the upgrade. */
	QSqlDatabase & mDB;

	Logger & mLogger;


	/** Creates a new instance of this object. */
	DatabaseUpgrade(Database & aDB, Logger & aLogger);

	/** Performs the whole upgrade on m_DB. */
	void execute();

	/** Returns the version stored in the DB.
	If the DB contains no versioning data, returns 0. */
	size_t getVersion();
};
