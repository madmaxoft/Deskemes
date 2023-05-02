#pragma once

#include <QWizardPage>

#include "../../Comm/DetectedDevices.hpp"





// fwd:
class ComponentCollection;
class NewDeviceWizard;
namespace Ui
{
	class PgNeedAuth;
}





/** The wizard page that informs the user of what to do when the device is not authorized. */
class PgNeedAuth:
	public QWizardPage
{
	using Super = QWizardPage;

	Q_OBJECT


public:

	explicit PgNeedAuth(ComponentCollection & aComponents, NewDeviceWizard & aParent);

	virtual ~PgNeedAuth() override;

	// QWizardPage overrides:
	virtual void initializePage() override;
	virtual bool isComplete() const override;
	virtual int nextId() const override;


protected Q_SLOTS:

	/** Called when a device status changes.
	Checks if the selected device is authorized; if so, enables the next wizard step. */
	void checkDeviceStatus(DetectedDevices::DevicePtr aDevice);


private:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The entire NewDevice wizard that stores the progress. */
	NewDeviceWizard & mParent;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::PgNeedAuth> mUI;


	/** Queries the ADB pub key and if found, displays it in the UI. */
	void initPubKeyDisplay();
};

