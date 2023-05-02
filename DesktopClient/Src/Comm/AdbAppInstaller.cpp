#include "AdbAppInstaller.hpp"





AdbAppInstaller::AdbAppInstaller():
	mAdbProcess(nullptr),
	mShouldAutoDeleteSelf(false)
{
}





void AdbAppInstaller::installAppFromFile(const QByteArray & aDeviceID, const QString & aApkFileName)
{
	if (mAdbProcess != nullptr)
	{
		throw std::logic_error("An installation is already in progress.");
	}
	mAdbProcess = new QProcess;
	connect(mAdbProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &AdbAppInstaller::adbProcessFinished);
	connect(mAdbProcess, &QProcess::errorOccurred, this, &AdbAppInstaller::adbProcessErrorOccurred);
	QStringList args;
	args << "-s" << aDeviceID << "install" << aApkFileName;
	mAdbProcess->start("adb", args);
}





void AdbAppInstaller::autoDeleteSelf()
{
	mShouldAutoDeleteSelf = true;
}





void AdbAppInstaller::adbProcessFinished(int aExitCode, QProcess::ExitStatus aExitStatus)
{
	// Free up the process instance, allow another install operation
	if (mAdbProcess == nullptr)
	{
		return;
	}
	mAdbProcess->deleteLater();
	auto proc = mAdbProcess;
	mAdbProcess = nullptr;

	if (aExitStatus == QProcess::NormalExit)
	{
		if (aExitCode == 0)
		{
			Q_EMIT installed();
		}
		else
		{
			// Report an error based on ADB output
			auto out = proc->readAllStandardOutput();
			auto err = proc->readAllStandardError();
			Q_EMIT errorOccurred(tr("The ADB process reported an error:\n%1\n%2").arg(QString::fromUtf8(err), QString::fromUtf8(out)));
		}
	}
	else
	{
		Q_EMIT errorOccurred(tr("The underlying ADB process crashed."));
	}

	// Auto-delete self, if requested to do so:
	if (mShouldAutoDeleteSelf)
	{
		deleteLater();
	}
}





void AdbAppInstaller::adbProcessErrorOccurred(QProcess::ProcessError aProcessError)
{
	// Free up the process instance, allow another install operation
	if (mAdbProcess == nullptr)
	{
		return;
	}
	mAdbProcess->deleteLater();
	mAdbProcess = nullptr;

	QString err;
	switch (aProcessError)
	{
		case QProcess::FailedToStart: err = tr("The underlying ADB process failed to start."); break;
		case QProcess::Crashed:       err = tr("The underlying ADB process crashed."); break;
		case QProcess::Timedout:      err = tr("The underlying ADB process seems to have frozen."); break;
		case QProcess::WriteError:    err = tr("Failed to send data to the underlying ADB process."); break;
		case QProcess::ReadError:     err = tr("Failed to read data from the underlying ADB process."); break;
		case QProcess::UnknownError:  err = tr("Unknown error in the underlying ADB process."); break;
	}
	Q_EMIT errorOccurred(err);

	// Auto-delete self, if requested to do so:
	if (mShouldAutoDeleteSelf)
	{
		deleteLater();
	}
}
