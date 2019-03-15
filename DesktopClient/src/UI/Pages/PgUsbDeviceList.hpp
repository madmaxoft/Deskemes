#pragma once

#include <memory>
#include <QWizardPage>
#include "../../ComponentCollection.hpp"
#include "../../Comm/DetectedDevices.hpp"





// fwd:
class UsbDeviceEnumerator;
namespace Ui
{
	class PgUsbDeviceList;
}





class PgUsbDeviceList:
	public QWizardPage
{
	using Super = QWizardPage;
	Q_OBJECT


public:

	explicit PgUsbDeviceList(ComponentCollection & aComponents, QWidget * aParent = nullptr);

	virtual ~PgUsbDeviceList() override;


private:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::PgUsbDeviceList> mUI;

	/** The device enumerator. */
	std::unique_ptr<UsbDeviceEnumerator> mDevEnum;

	/** The detected devices list, model for the UI. */
	DetectedDevices mDetectedDevices;


	// QWizardPage overrides:
	virtual void initializePage() override;
};
