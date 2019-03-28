#pragma once

#include <memory>
#include <QWizard>
#include "../../Comm/DetectedDevices.hpp"





// fwd:
class ComponentCollection;
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

	explicit PgTcpDeviceList(ComponentCollection & aComponents, QWidget * aParent);

	virtual ~PgTcpDeviceList() override;


	// QWizardPage overrides:
	virtual void initializePage() override;

private:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::PgTcpDeviceList> mUI;

	/** The detected devices list, model for the UI. */
	std::shared_ptr<DetectedDevices> mDetectedDevices;
};
