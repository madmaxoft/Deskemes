#include "InstallConfiguration.hpp"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QDateTime>
#include <QNetworkInterface>
#include "Settings.hpp"
#include "Utils.hpp"





#ifdef _WIN32
	// Needed for NTFS permission checking, as documented in QFileInfo
	extern Q_CORE_EXPORT int qt_ntfs_permission_lookup;
#endif  // _WIN32





InstallConfiguration::InstallConfiguration(ComponentCollection & aComponents):
	ComponentSuper(aComponents),
	mDataPath(detectDataPath())
{
	qDebug() << "Using data path " << mDataPath;
}





void InstallConfiguration::loadFromSettings()
{
	loadOrGeneratePublicID();
	mFriendlyName = Settings::loadValue("InstallConfiguration", "FriendlyName", "").toString();
	if (mFriendlyName.isEmpty())
	{
		auto hex = QString::fromUtf8(Utils::toHex(mPublicID.left(4)));
		mFriendlyName = QString::fromUtf8("Deskemes_%1").arg(hex);
		setFriendlyName(mFriendlyName);
	}
	mAvatar = Settings::loadValue("InstallConfiguration", "Avatar", {});
}





void InstallConfiguration::setFriendlyName(const QString & aFriendlyName)
{
	mFriendlyName = aFriendlyName;
	Settings::saveValue("InstallConfiguration", "FriendlyName", mFriendlyName);
}





QString InstallConfiguration::detectDataPath()
{
	// If the current folder is writable, use that as the data location ("portable" version):
	if (isDataPathSuitable("."))
	{
		return "";
	}

	// Try the local app data folder:
	auto writableData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	if (isDataPathSuitable(writableData))
	{
		if (writableData.right(1) != "/")
		{
			writableData += "/";
		}
		return writableData;
	}

	// No suitable location found
	throw RuntimeError("Cannot find a suitable location to store the data.");
}





bool InstallConfiguration::isDataPathSuitable(const QString & aFolder)
{
	QFileInfo folder(aFolder);
	if (!folder.exists())
	{
		QDir d;
		if (!d.mkpath(aFolder))
		{
			// Folder doesn't exist and cannot be created.
			return false;
		}
	}

	#ifdef _WIN32
		qt_ntfs_permission_lookup++;  // Turn NTFS permission checking on
	#endif

	auto res = folder.isWritable();

	#ifdef _WIN32
		qt_ntfs_permission_lookup--;  // Turn NTFS permission checking off
	#endif

	return res;
}





void InstallConfiguration::loadOrGeneratePublicID()
{
	mPublicID = Settings::loadValue("InstallConfiguration", "PublicID", {}).toByteArray();
	if (mPublicID.size() >= 16)
	{
		return;
	}

	// Generate and store a new PublicID:
	// Use a crypto hash of current time, network addresses and username-related paths to provide at least some entropy:
	qDebug() << "Public ID not set or invalid, creating a new one";
	QCryptographicHash hash(QCryptographicHash::Sha512);
	auto time = QDateTime::currentMSecsSinceEpoch();
	hash.addData(reinterpret_cast<const char *>(&time), sizeof(time));
	auto netAddrs = QNetworkInterface::allAddresses();
	for (const auto & addr: netAddrs)
	{
		hash.addData(addr.toString().toUtf8());
	}
	hash.addData(QStandardPaths::writableLocation(QStandardPaths::HomeLocation).toUtf8());
	mPublicID = hash.result();
	qDebug() << "Public ID generated: " << mPublicID;
	Settings::saveValue("InstallConfiguration", "PublicID", mPublicID);
}
