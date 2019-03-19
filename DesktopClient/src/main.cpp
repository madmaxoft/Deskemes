#include <memory>
#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QDebug>
#include <QMessageBox>
#include <QFile>
#include "BackgroundTasks.hpp"
#include "ComponentCollection.hpp"
#include "DebugLogger.hpp"
#include "Device.hpp"
#include "Devices.hpp"
#include "InstallConfiguration.hpp"
#include "Settings.hpp"
#include "DB/DatabaseBackup.hpp"
#include "DB/Database.hpp"
#include "UI/WndDevices.hpp"
#include "Comm/DetectedDevices.hpp"
#include "Comm/TcpListener.hpp"
#include "Comm/UdpBroadcaster.hpp"





// From http://stackoverflow.com/a/27807379
// When linking statically with CMake + MSVC, we need to add the Qt plugins' initializations:
#if defined(_MSC_VER) && defined(FORCE_STATIC_RUNTIME)
	#include <QtPlugin>
	Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
	Q_IMPORT_PLUGIN(QSQLiteDriverPlugin)
#endif





/** Loads translation for the specified locale from all reasonable locations.
Returns true if successful, false on failure.
Tries: resources, <curdir>/translations, <exepath>/translations. */
static bool tryLoadTranslation(QTranslator & a_Translator, const QLocale & a_Locale)
{
	static const QString exePath = QCoreApplication::applicationDirPath();

	if (a_Translator.load("Deskemes_" + a_Locale.name(), ":/translations"))
	{
		qDebug() << "Loaded translation " << a_Locale.name() << " from resources";
		return true;
	}
	if (a_Translator.load("Deskemes_" + a_Locale.name(), "translations"))
	{
		qDebug() << "Loaded translation " << a_Locale.name() << " from current folder";
		return true;
	}
	if (a_Translator.load("Deskemes_" + a_Locale.name(), exePath + "/translations"))
	{
		qDebug() << "Loaded translation " << a_Locale.name() << " from exe folder";
		return true;
	}
	return false;
}





void initTranslations(QApplication & a_App)
{
	auto translator = std::make_unique<QTranslator>();
	auto locale = QLocale::system();
	if (!tryLoadTranslation(*translator, locale))
	{
		qWarning() << "Could not load translations for locale " << locale.name() << ", trying all UI languages " << locale.uiLanguages();
		if (!translator->load(locale, "Deskemes", "_", "translations"))
		{
			qWarning() << "Could not load translations for " << locale;
			return;
		}
	}
	qDebug() << "Translator empty: " << translator->isEmpty();
	a_App.installTranslator(translator.release());
}





int main(int argc, char *argv[])
{
	// Initialize the DebugLogger:
	DebugLogger::get();

	QApplication app(argc, argv);

	try
	{
		// Initialize translations:
		initTranslations(app);

		// Initialize singletons / subsystems:
		BackgroundTasks::get();
		qRegisterMetaType<DevicePtr>();
		qRegisterMetaType<DetectedDevices::DeviceStatusList>();
		auto instConf = std::make_shared<InstallConfiguration>();
		Settings::init(instConf->dataLocation("Deskemes.ini"));
		instConf->loadFromSettings();

		// Create the main app objects:
		ComponentCollection cc;
		cc.addComponent(instConf);
		auto mainDB      = cc.addNew<Database>(cc);
		auto devs        = cc.addNew<Devices>(cc);
		auto broadcaster = cc.addNew<UdpBroadcaster>(cc);
		auto listener    = cc.addNew<TcpListener>();

		listener->start();
		broadcaster->start();

		// Connect the main objects together:

		// Load the DB:
		auto dbFile = instConf->dbFileName();
		DatabaseBackup::dailyBackupOnStartup(dbFile, instConf->dbBackupsFolder());
		mainDB->open(dbFile);

		// Show the UI:
		WndDevices w(cc);
		if (Settings::loadValue("main", "MainWindowVisible", true).toBool())
		{
			w.show();
		}
		else
		{
			w.hide();
		}

		// Run the app:
		auto res = app.exec();

		// Stop all background tasks:
		BackgroundTasks::get().stopAll();

		return res;
	}
	catch (const std::exception & exc)
	{
		QMessageBox::warning(
			nullptr,
			QApplication::tr("Deskemes: Fatal error"),
			QApplication::tr("Deskemes has detected a fatal error:\n\n%1").arg(exc.what())
		);
		return -1;
	}
	catch (...)
	{
		QMessageBox::warning(
			nullptr,
			QApplication::tr("Deskemes: Fatal error"),
			QApplication::tr("Deskemes has detected an unknown fatal error. Use a debugger to view detailed runtime log.")
		);
		return -1;
	}
}
