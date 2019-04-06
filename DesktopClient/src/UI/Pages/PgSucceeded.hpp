#pragma once

#include <memory>
#include <QWizardPage>





// fwd:
class ComponentCollection;
class NewDeviceWizard;
namespace Ui
{
	class PgSucceeded;
}





class PgSucceeded:
	public QWizardPage
{
	using Super = QWizardPage;

	Q_OBJECT


public:

	explicit PgSucceeded(ComponentCollection & aComponents, NewDeviceWizard & aParent);

	virtual ~PgSucceeded() override;


private:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The entire NewDevice wizard that stores the progress. */
	NewDeviceWizard & mParent;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::PgSucceeded> mUI;
};
