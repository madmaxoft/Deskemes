#include "Database.hpp"
#include <cassert>
#include <set>
#include <array>
#include <fstream>
#include <random>
#include <atomic>
#include <QDebug>
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
	SqlTransaction(QSqlDatabase & a_DB):
		m_DB(a_DB),
		m_IsActive(a_DB.transaction())
	{
		if (!m_IsActive)
		{
			throw RuntimeError("DB doesn't support transactions: %1", m_DB.lastError());
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
				qWarning() << "DB transaction rollback failed: " << m_DB.lastError();
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
			throw RuntimeError("DB transaction not started");
		}
		if (!m_DB.commit())
		{
			throw LogicError("DB transaction commit failed: %1", m_DB.lastError());
		}
		m_IsActive = false;
	}


protected:

	QSqlDatabase & m_DB;
	bool m_IsActive;
};





////////////////////////////////////////////////////////////////////////////////
// Database:

Database::Database(ComponentCollection & aComponents):
	mComponents(aComponents)
{
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
		throw RuntimeError(tr("Cannot open the DB file: %1"), mDatabase.lastError());
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
				throw RuntimeError(tr("Cannot open DB, it is from a newer version %1, this program can only handle up to version %2"),
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
				DatabaseBackup::backupBeforeUpgrade(a_DBFileName, version, backupFolder);

				// Reopen the DB:
				mDatabase = QSqlDatabase::addDatabase("QSQLITE", connName);
				mDatabase.setDatabaseName(a_DBFileName);
				if (!mDatabase.open())
				{
					throw RuntimeError(tr("Cannot open the DB file: %1"), mDatabase.lastError());
				}
			}
		}
	}

	// Turn on foreign keys:
	auto query = mDatabase.exec("PRAGMA foreign_keys = on");
	if (query.lastError().type() != QSqlError::NoError)
	{
		qWarning() << "Turning foreign keys on failed: " << query.lastError();
		throw RuntimeError(tr("Cannot initialize the DB keys: %1"), mDatabase.lastError());
	}

	// Upgrade the DB to the latest version:
	DatabaseUpgrade::upgrade(*this);
}
