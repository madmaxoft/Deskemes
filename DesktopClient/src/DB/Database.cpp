#include "Database.hpp"
#include <cassert>
#include <set>
#include <array>
#include <fstream>
#include <random>
#include <atomic>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlField>
#include <QThread>
#include <QApplication>
#include <QFile>
#include "../InstallConfiguration.hpp"
#include "../Exception.hpp"
#include "DatabaseUpgrade.hpp"
#include "DatabaseBackup.hpp"





/** Implements a RAII-like behavior for transactions.
Unless explicitly committed, a transaction is rolled back upon destruction of this object. */
class SqlTransaction
{
public:

	/** Starts a transaction.
	If a transaction cannot be started, logs and throws a RuntimeError. */
	SqlTransaction(QSqlDatabase & a_DB, Logger & aLogger):
		m_DB(a_DB),
		m_IsActive(a_DB.transaction()),
		mLogger(aLogger)
	{
		if (!m_IsActive)
		{
			throw RuntimeError(mLogger, "DB doesn't support transactions: %1", m_DB.lastError());
		}
	}


	/** Rolls back the transaction, unless it has been committed.
	If the transaction fails to roll back, an error is logged (but nothing thrown). */
	~SqlTransaction()
	{
		if (m_IsActive)
		{
			if (!m_DB.rollback())
			{
				mLogger.log("DB transaction rollback failed: %1", m_DB.lastError());
				return;
			}
		}
	}


	/** Commits the transaction.
	If a transaction wasn't started, or is already committed, logs and throws a LogicError.
	If the transaction fails to commit, throws a RuntimeError. */
	void commit()
	{
		if (!m_IsActive)
		{
			throw RuntimeError(mLogger, "DB transaction not started");
		}
		if (!m_DB.commit())
		{
			throw LogicError(mLogger, "DB transaction commit failed: %1", m_DB.lastError());
		}
		m_IsActive = false;
	}


protected:

	QSqlDatabase & m_DB;
	bool m_IsActive;
	Logger & mLogger;
};





////////////////////////////////////////////////////////////////////////////////
// Database::DBConnection

Database::DBConnection::DBConnection(Database & aParent):
	mParent(aParent)
{
	mParent.mMtxConnection.lock();
	mParent.mDatabase.transaction();
}





Database::DBConnection::~DBConnection()
{
	mParent.mDatabase.commit();
	mParent.mMtxConnection.unlock();
}





QSqlQuery Database::DBConnection::query(const QString & aQueryString)
{
	QSqlQuery res(mParent.mDatabase);
	if (!res.prepare(aQueryString))
	{
		throw DBQueryError(mParent.mLogger, "Failed to prepare query: %1 (query \"%2\")", res.lastError().text(), aQueryString);
	}
	return res;
}





////////////////////////////////////////////////////////////////////////////////
// Database:

Database::Database(ComponentCollection & aComponents):
	ComponentSuper(aComponents),
	mLogger(aComponents.logger("DB"))
{
	requireForStart(ComponentCollection::ckInstallConfiguration);
}





void Database::start()
{
	mLogger.log("Starting the Database component");
	auto instConf = mComponents.get<InstallConfiguration>();
	auto dbFile = instConf->dbFileName();
	DatabaseBackup::dailyBackupOnStartup(dbFile, instConf->dbBackupsFolder(), mLogger);
	open(dbFile);
}





void Database::open(const QString & a_DBFileName)
{
	assert(!mDatabase.isOpen());  // Opening another DB is not allowed

	static std::atomic<int> counter(0);
	auto connName = QString::fromUtf8("DB%1").arg(counter.fetch_add(1));
	mDatabase = QSqlDatabase::addDatabase("QSQLITE", connName);
	mDatabase.setDatabaseName(a_DBFileName);
	if (!mDatabase.open())
	{
		throw RuntimeError(mLogger, tr("Cannot open the DB file: %1"), mDatabase.lastError());
	}

	// Check DB version, if upgradeable, make a backup first:
	{
		auto query = std::make_unique<QSqlQuery>("SELECT MAX(Version) AS Version FROM Version", mDatabase);
		if (query->first())
		{
			auto version = query->record().value("Version").toULongLong();
			auto maxVersion = DatabaseUpgrade::currentVersion();
			query.reset();
			if (version > maxVersion)
			{
				throw RuntimeError(mLogger, tr("Cannot open DB, it is from a newer version %1, this program can only handle up to version %2"),
					version, maxVersion
				);
			}
			if (version < maxVersion)
			{
				// Close the DB:
				mDatabase.close();
				QSqlDatabase::removeDatabase(connName);

				// Backup:
				auto backupFolder = mComponents.get<InstallConfiguration>()->dbBackupsFolder();
				DatabaseBackup::backupBeforeUpgrade(a_DBFileName, version, backupFolder, mLogger);

				// Reopen the DB:
				mDatabase = QSqlDatabase::addDatabase("QSQLITE", connName);
				mDatabase.setDatabaseName(a_DBFileName);
				if (!mDatabase.open())
				{
					throw RuntimeError(mLogger, tr("Cannot open the DB file: %1"), mDatabase.lastError());
				}
			}
		}
	}

	// Turn on foreign keys:
	auto query = mDatabase.exec("PRAGMA foreign_keys = on");
	if (query.lastError().type() != QSqlError::NoError)
	{
		throw RuntimeError(mLogger, tr("Failed to turn on foreign keys: %1"), query.lastError());
	}

	// Upgrade the DB to the latest version:
	DatabaseUpgrade::upgrade(*this, mLogger);
}





Database::DBConnection Database::connection()
{
	return Database::DBConnection(*this);
}
