#pragma once

#include <memory>
#include <QWizardPage>





// fwd:
class ComponentCollection;
class NewDeviceWizard;
namespace Ui
{
	class PgPairConfirm;
}





class PgPairConfirm:
	public QWizardPage
{
	using Super = QWizardPage;

	Q_OBJECT


public:

	explicit PgPairConfirm(ComponentCollection & aComponents, NewDeviceWizard & aParent);

	virtual ~PgPairConfirm() override;

	// QWizardPage overrides:
	virtual int nextId() const override;
	virtual bool isComplete() const override;
	virtual void initializePage() override;
	virtual void cleanupPage() override;
	virtual bool validatePage() override;


private:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The entire NewDevice wizard that stores the progress. */
	NewDeviceWizard & mParent;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::PgPairConfirm> mUI;
};
