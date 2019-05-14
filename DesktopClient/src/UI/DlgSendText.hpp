#pragma once

#include <memory>
#include <QDialog>





// fwd:
class ComponentCollection;
namespace Ui
{
	class DlgSendText;
}





class DlgSendText:
	public QDialog
{
	using Super = QDialog;
	Q_OBJECT


public:

	explicit DlgSendText(ComponentCollection & aComponents, QWidget * aParent = nullptr);

	virtual ~DlgSendText() override;


private:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::DlgSendText> mUI;


private slots:

	/** Updates the char counter, schedules an update to the message counter, enables / disables the Send button. */
	void updateCounters();

	/** Sends the message and closes the dialog. */
	void sendAndClose();
};
