#pragma once

#include <QString>
#include "ComponentCollection.hpp"
#include "Optional.hpp"





/** Provides information that is specific to this installation of the program:
- writable data location
- DB location
- backups location
- the instance ID of this installation */
class InstallConfiguration:
	public ComponentCollection::Component<ComponentCollection::ckInstallConfiguration>
{
	using ComponentSuper = ComponentCollection::Component<ComponentCollection::ckInstallConfiguration>;


public:

	/** Initializes the paths.
	Throws a RuntimeError if no suitable path was found. */
	InstallConfiguration(ComponentCollection & aComponents);

	/** Returns the path where the specified file (with writable data) / folder should be stored. */
	QString dataLocation(const QString & aFileName) const { return mDataPath + aFileName; }

	/** Returns the filename of the main DB. */
	QString dbFileName() const { return dataLocation("Deskemes.sqlite"); }

	/** Returns the path to the folder into which DB backups should be stored. */
	QString dbBackupsFolder() const { return dataLocation("backups/"); }

	/** Returns the path to the folder into which the logs should be written. */
	QString logsFolder() const { return dataLocation("logs/"); }

	/** Loads the values that are stored in the Settings object.
	This is only used during app initialization. */
	void loadFromSettings();

	/** Stores the specified friendly name. */
	void setFriendlyName(const QString & aFriendlyName);

	// Simple getters:
	const QByteArray & publicID() const { return mPublicID; }
	const QString & friendlyName() const { return mFriendlyName; }
	const Optional<QByteArray> & avatar() const { return mAvatar; }


protected:

	/** The base path where Deskemes should store its data (writable).
	If nonempty, includes the trailing slash. */
	QString mDataPath;

	/** The public ID of the Deskemes instance, as read from the Settings, and used as identification
	for communicating with the devices. */
	QByteArray mPublicID;

	/** The user-given name of this Deskemes instance, to be shown by devices when pairing. */
	QString mFriendlyName;

	/** The user-given avatar of this Deskemes instance, to be shown by devices when pairing. */
	Optional<QByteArray> mAvatar;


	/** Returns the folder to use for m_DataPath.
	To be called from the constructor.
	Throws a RuntimeError if no suitable path was found. */
	QString detectDataPath();

	/** Returns true if the specified path is suitable for data path.
	Checks that the path is writable.
	If the path doesn't exist, creates it. */
	bool isDataPathSuitable(const QString & aFolder);

	/** Loads the public ID from the Settings, or generates a new one if not present. */
	void loadOrGeneratePublicID();
};
