#pragma once

#include <QWizardPage>

#include "../../Comm/DetectedDevices.hpp"





// fwd:
class ComponentCollection;
class NewDeviceWizard;
namespace Ui
{
	class PgNeedApp;
}





/** The wizard page that shows the information about needing to install the app, and offering to install the app for the user. */
class PgNeedApp:
	public QWizardPage
{
	using Super = QWizardPage;

	Q_OBJECT


public:

	explicit PgNeedApp(ComponentCollection & aComponents, NewDeviceWizard & aParent);

	virtual ~PgNeedApp() override;

	// QWizardPage overrides:
	virtual void initializePage() override;
	virtual bool isComplete() const override;
	virtual int nextId() const override;


protected Q_SLOTS:

	/** Called when a device status changes.
	Checks if the selected device has an app installed; if so, jumps to the next wizard step. */
	void checkDeviceStatus();

	/** Asks ADB to install the app on the selected device. */
	void tryInstallApp();

	/** Asks ADB to start an intent opening the browser to the app package online. */
	void openBrowserWithAppWebPage();

	/** App install was reported as successful, hide the progressbar, show a success message. */
	void onAppInstalled();

	/** App install failed with the specified error, hide the progressbar, show the error message. */
	void onAppInstallFailed(QString aErrorDesc);


private:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The entire NewDevice wizard that stores the progress. */
	NewDeviceWizard & mParent;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::PgNeedApp> mUI;
};

