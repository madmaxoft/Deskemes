#include "ChannelSmsSend.hpp"
#include "../../Utils.hpp"





ChannelSmsSend::ChannelSmsSend(Connection & aConnection):
	Super(aConnection)
{
}





void ChannelSmsSend::sendSms(const QString & aRecipient, const QString & aText)
{
	mConnection.logger().log("Sending an sms, recipient = \"%1\", text = \"%2\".", aRecipient, aText);
	QByteArray msg;
	msg.append("send");
	Utils::writeBE16Lstring(msg, aRecipient.toUtf8());
	Utils::writeBE16Lstring(msg, aText.toUtf8());
	sendMessage(msg);
}





std::vector<QString> ChannelSmsSend::divideMessage(const QString & aText)
{
	QByteArray msg;
	msg.append("dvde");
	Utils::writeBE16Lstring(msg, aText.toUtf8());
	sendMessage(msg);

	// TODO: Wait for result
	mConnection.logger().log("ChannelSmsSend::divideMessage(): NOT IMPLEMENTED YET");

	return {};
}





void ChannelSmsSend::processIncomingMessage(const QByteArray & aMessage)
{
	Q_UNUSED(aMessage);
	// TODO
}
