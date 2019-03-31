#pragma once

#include <vector>
#include <memory>
#include <QObject>
#include <QSqlDatabase>
#include <QMutex>
#include <QSqlQuery>
#include "../ComponentCollection.hpp"





/** The storage for all data that is persisted across sessions.
The database file stores all user data and provides encryption / decryption.
Clients of this class take the SQL connection and issue their own queries on the database. */
class Database:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckDatabase>
{
	Q_OBJECT
	using Super = QObject;


public:

	/** An exception that is thrown when DB query fails. */
	class DBQueryError:
		public RuntimeError
	{
	public:
		using RuntimeError::RuntimeError;
	};


	/** Wrapper for the DB connection, used by the clients to query and modify data.
	Only one connection is ever active at a time, to prevent threading issues.
	A DB transaction is started at the creation of the class, and committed at destruction.
	Get an instance through Database::connection(), and destroy the object as soon as the DB is not needed. */
	class DBConnection
	{
		friend class ::Database;

		/** Creates a new instance and locks aParent's mMtxConnection. */
		DBConnection(Database & aParent);


	public:

		/** Destroys this instance and unlocks mParent's mMtxConnection. */
		~DBConnection();

		/** Creates a new query and prepares it using the specified query string.
		Throws a DBQueryError if query preparation fails. */
		QSqlQuery query(const QString & aQueryString);


	protected:

		/** The Database object that provided this connection. */
		Database & mParent;
	};


	Database(ComponentCollection & aComponents);

	/** Opens the specified SQLite file to provide the data backstore.
	Only one DB can ever be open.
	TODO: Encryption support. */
	void open(const QString & a_DBFileName);

	/** Returns a connection to the DB that can be used to query and modify data.
	Only one connection is ever active at a time, to prevent threading issues,
	so the client needs to destroy the returned connection as soon as it's done working with the DB. */
	DBConnection connection();


protected:

	friend class DatabaseUpgrade;


	/** The components of the entire program. */
	ComponentCollection & mComponents;

	/** The DB connection .*/
	QSqlDatabase mDatabase;

	/** The mutex that is used to sequentialize access to the database / DBConnection. */
	QMutex mMtxConnection;


	/** Returns the internal QSqlDatabase object. */
	QSqlDatabase & database() { return mDatabase; }
};
