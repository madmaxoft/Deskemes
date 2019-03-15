#pragma once

#include <memory>
#include <QWizard>




// fwd:
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

	explicit PgTcpDeviceList(QWidget * aParent);

	virtual ~PgTcpDeviceList() override;


	// QWizardPage overrides:
	virtual void initializePage() override;

private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::PgTcpDeviceList> mUI;
};
