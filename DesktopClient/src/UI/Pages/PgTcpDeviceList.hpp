#pragma once

#include <memory>
#include <QWizard>
#include "../../Comm/DetectedDevices.hpp"





// fwd:
class ComponentCollection;
class NewDeviceWizard;
namespace Ui
{
	class PgTcpDeviceList;
}





/** The wizard page that shows the devices available through TCP, while detection runs in the background. */
class PgTcpDeviceList:
	public QWizardPage
{
	using Super = QWizardPage;

	Q_OBJECT


public:

	explicit PgTcpDeviceList(ComponentCollection & aComponents, NewDeviceWizard & aParent);

	virtual ~PgTcpDeviceList() override;


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
	std::unique_ptr<Ui::PgTcpDeviceList> mUI;

	/** The detected devices list, model for the UI. */
	std::shared_ptr<DetectedDevices> mDetectedDevices;


	/** Returns the device that is selected in the UI.
	Returns nullptr if no selection. */
	DetectedDevices::DevicePtr selectedDevice() const;


protected slots:

	/** The user has double-clicked a device, go to the next page with that device. */
	void deviceDoubleClicked(const QModelIndex & aIndex);
};
