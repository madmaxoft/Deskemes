#pragma once

#include <memory>
#include <QWizardPage>





// fwd:
namespace Ui
{
	class PgConnectionType;
}





/** The initial page of the "new device" wizard, asking for the connection type. */
class PgConnectionType:
	public QWizardPage
{
	using Super = QWizardPage;

	Q_OBJECT

public:

	explicit PgConnectionType(QWidget * aParent = nullptr);

	virtual ~PgConnectionType() override;


	// QWizardPage overrides:
	virtual int nextId() const override;


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::PgConnectionType> mUI;
};
