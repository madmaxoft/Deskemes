#include "DatabaseBackup.hpp"
#include <QDate>
#include <QFileInfo>
#include <QDir>
#include "../Exception.hpp"





void DatabaseBackup::dailyBackupOnStartup(
	const QString & a_DBFileName,
	const QString & a_BackupFolder,
	Logger & aLogger
)
{
	aLogger.log("Creating daily backup...");
	// If the DB file is not existent, there's nothing to back up:
	if (!QFile::exists(a_DBFileName))
	{
		aLogger.log("Skipping daily backup, there's no DB file %1 yet.", a_DBFileName);
		return;
	}

	auto now = QDate::currentDate();
	auto dstFileName = a_BackupFolder + QString("%1/%1-%2-%3.sqlite")
		.arg(now.year())
		.arg(QString::number(now.month()), 2, '0')
		.arg(QString::number(now.day()), 2, '0');

	QFileInfo fi(dstFileName);
	if (fi.exists())
	{
		aLogger.log("Skipping daily backup, already made one today");
		return;
	}
	if(!fi.absoluteDir().mkpath(fi.absolutePath()))
	{
		throw RuntimeError(aLogger, tr("Cannot create folder for daily backups: %1"), fi.absolutePath());
	}
	if (!QFile::copy(a_DBFileName, dstFileName))
	{
		throw RuntimeError(aLogger, tr("Cannot create a daily DB backup %1"), dstFileName);
	}
	aLogger.log("Daily backup created");
}





void DatabaseBackup::backupBeforeUpgrade(
	const QString & a_DBFileName,
	size_t a_CurrentVersion,
	const QString & a_BackupFolder,
	Logger & aLogger
)
{
	aLogger.log("Backing up before an upgrade...");
	auto now = QDate::currentDate();
	auto dstFileName = a_BackupFolder + QString("%1/%1-%2-%3-ver%4.sqlite")
		.arg(now.year())
		.arg(QString::number(now.month()), 2, '0')
		.arg(QString::number(now.day()), 2, '0')
		.arg(static_cast<qulonglong>(a_CurrentVersion));

	QFileInfo fi(dstFileName);
	if (fi.exists())
	{
		throw Exception(aLogger, tr("Cannot backup DB before upgrade, destination file %1 already exists."), dstFileName);
	}
	if(!fi.absoluteDir().mkpath(fi.absolutePath()))
	{
		throw RuntimeError(aLogger, tr("Cannot create the folder for the pre-upgrade backup: %1"), fi.absolutePath());
	}
	if (!QFile::copy(a_DBFileName, dstFileName))
	{
		throw RuntimeError(aLogger, tr("Cannot create the pre-upgrade DB backup %1"), dstFileName);
	}
	aLogger.log("Pre-upgrade DB backup %1 created", dstFileName);
}
