#pragma once

#include <vector>
#include <memory>
#include <QObject>
#include <QSqlDatabase>
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

	Database(ComponentCollection & aComponents);

	/** Opens the specified SQLite file and reads its contents into this object.
	Keeps the DB open for subsequent immediate updates.
	Only one DB can ever be open.
	TODO: Encryption support. */
	void open(const QString & a_DBFileName);


protected:

	friend class DatabaseUpgrade;


	/** The components of the entire program. */
	ComponentCollection & mComponents;

	/** The DB connection .*/
	QSqlDatabase mDatabase;


	/** Returns the internal QSqlDatabase object. */
	QSqlDatabase & database() { return mDatabase; }
};
