#pragma once

#include <QObject>
#include <QProcess>





/** Takes care of installing apps through ADB asynchronously.
A single instance can install one app at a time.
Progress, success or failure is reported using Qt signals. */
class AdbAppInstaller:
	public QObject
{
	using Super = QObject;
	Q_OBJECT

public:

	explicit AdbAppInstaller();

	/** Tries to install the app from the specified APK to the specified device.
	Works asynchonously in the background.
	Emits either the installed() or the errorOccurred() signal. */
	void installAppFromFile(const QByteArray & aDeviceID, const QString & aApkFileName);

	/** After this call, when the installation is finished (or error occurs), this instance auto-deletes itself. */
	void autoDeleteSelf();


Q_SIGNALS:

	/** Emitted once the APK file is installed successfully on the device. */
	void installed();

	/** Emittd is the installation fails, either by not having an ADB available or it reports a problem. */
	void errorOccurred(QString aErrorDesc);


private:

	/** The ADB process handling the actual installation.
	nullptr while there's no installation in progress. */
	QProcess * mAdbProcess;

	/** If true, the object will auto-delete self once the installation finishes or an error occurs. */
	bool mShouldAutoDeleteSelf;


private Q_SLOTS:

	/** Invoked by mAdbProcess when the ADB process finishes. */
	void adbProcessFinished(int aExitCode, QProcess::ExitStatus aExitStatus);

	/** Invoked by mAdbProcess when the ADB process reports an error. */
	void adbProcessErrorOccurred(QProcess::ProcessError aProcessError);
};

