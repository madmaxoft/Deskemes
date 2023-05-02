#include "DlgSendText.hpp"
#include <QDebug>
#include "ui_DlgSendText.h"
#include "../Settings.hpp"
#include "../ComponentCollection.hpp"
#include "../DeviceMgr.hpp"
#include "../Comm/Channels/ChannelSmsSend.hpp"





DlgSendText::DlgSendText(ComponentCollection & aComponents, QWidget * aParent):
	Super(aParent),
	mComponents(aComponents),
	mUI(new Ui::DlgSendText)
{
	mUI->setupUi(this);
	Settings::loadWindowPos("DlgSendText", *this);

	// Fill in the "from" devices:
	auto devMgr = mComponents.get<DeviceMgr>();
	auto cb = mUI->cbFrom;
	for (const auto & dev: devMgr->devices())
	{
		cb->addItem(dev->friendlyName(), QVariant::fromValue(dev));
	}

	// Connect the signals:
	connect(mUI->teText, &QTextEdit::textChanged, this, &DlgSendText::updateCounters);
	connect(mUI->btnCancel, &QPushButton::pressed, this, &QDialog::reject);
	connect(mUI->btnSend,   &QPushButton::pressed, this, &DlgSendText::sendAndClose);

	updateCounters();
}





DlgSendText::~DlgSendText()
{
	Settings::saveWindowPos("DlgSendText", *this);
}





void DlgSendText::updateCounters()
{
	auto text = mUI->teText->toPlainText();
	auto numChars = text.length();
	auto numMessages = numChars / 160;  // TODO: proper count using the `dvde` command
	mUI->lblCharCount->setText(tr("Characters: %1 / Messages: %2").arg(numChars).arg(numMessages));
	mUI->btnSend->setEnabled(numChars > 0);
}





void DlgSendText::sendAndClose()
{
	// Send the message:
	auto dev = mUI->cbFrom->currentData().value<DevicePtr>();
	if (dev == nullptr)
	{
		qWarning() << "Cannot send, unknown source device";
		return;
	}
	auto conn = dev->connections()[0];
	auto sendChannel = std::make_shared<ChannelSmsSend>(*conn);
	auto to = mUI->cbTo->currentText();
	auto text = mUI->teText->toPlainText();
	connect(sendChannel.get(), &ChannelSmsSend::opened,
		[to, text, sendChannel]()
		{
			qDebug() << "Sending SMS to " << to << ": " << text;
			sendChannel->sendSms(to, text);
		}
	);
	conn->openChannel(sendChannel, "sms.send");

	accept();
}
