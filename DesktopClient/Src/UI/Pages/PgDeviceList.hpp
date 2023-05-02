#pragma once

#include <memory>
#include <QWizard>
#include "../../Comm/DetectedDevices.hpp"





// fwd:
class ComponentCollection;
class NewDeviceWizard;
class DetectedDevicesModel;
namespace Ui
{
	class PgDeviceList;
}





/** The wizard page that shows the available devices, while detection runs in the background. */
class PgDeviceList:
	public QWizardPage
{
	using Super = QWizardPage;

	Q_OBJECT


public:

	explicit PgDeviceList(ComponentCollection & aComponents, NewDeviceWizard & aParent);

	virtual ~PgDeviceList() override;


	// QWizardPage overrides:
	virtual void initializePage() override;
	virtual bool isComplete() const override;
	virtual int nextId() const override;


private:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The entire NewDevice wizard that stores the progress. */
	NewDeviceWizard & mParent;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::PgDeviceList> mUI;

	/** The detected devices model for the UI. */
	std::unique_ptr<DetectedDevicesModel> mDetectedDevicesModel;


	/** Returns the device that is selected in the UI.
	Returns nullptr if no selection. */
	DetectedDevices::DevicePtr selectedDevice() const;


protected Q_SLOTS:

	/** The user has double-clicked a device, go to the next page with that device. */
	void deviceDoubleClicked(const QModelIndex & aIndex);
};
