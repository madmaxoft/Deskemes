#pragma once

#include <QObject>
#include <QString>
#include "../Logger.hpp"





/** A namespace-class for functions performing DB backup on various occasions. */
class DatabaseBackup:
	public QObject
{
	Q_OBJECT

public:
	/** If the DB hasn't been backed up today, makes a backup.
	Throws a RuntimeError if a backup is to be made, but it fails. */
	static void dailyBackupOnStartup(
		const QString & a_DBFileName,
		const QString & a_BackupFolder,
		Logger & aLogger
	);


	/** Makes a backup of the DB before upgrading.
	a_CurrentVersion is the current DB version (before upgrade).
	Throws a RuntimeError if the backup fails.
	Fails if the destination file already exists. */
	static void backupBeforeUpgrade(const QString & a_DBFileName,
		size_t a_CurrentVersion,
		const QString & a_BackupFolder,
		Logger & aLogger
	);
};
