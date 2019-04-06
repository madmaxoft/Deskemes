#pragma once

#include <memory>
#include <QWizardPage>





// fwd:
class ComponentCollection;
class NewDeviceWizard;
class Connection;
namespace Ui
{
	class PgPairingInProgress;
}





class PgPairingInProgress:
	public QWizardPage
{
	using Super = QWizardPage;

	Q_OBJECT


public:

	explicit PgPairingInProgress(ComponentCollection & aComponents, NewDeviceWizard & aParent);

	virtual ~PgPairingInProgress() override;

	// QWizardPage overrides:
	virtual void initializePage() override;
	virtual int nextId() const override { return mNextPage; }
	virtual bool isComplete() const override { return (mNextPage >= 0); }

private:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The entire NewDevice wizard that stores the progress. */
	NewDeviceWizard & mParent;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::PgPairingInProgress> mUI;

	/** The page that will be next in the wizard. Updated when the connection changes state.
	If -1, the Next button is disabled. */
	int mNextPage;


private slots:

	/** The connection has updated its state.
	Checks if the connection has successfully negotiated TLS; if so, advances to the next page. */
	void connStateChanged(Connection * aConnection);
};
