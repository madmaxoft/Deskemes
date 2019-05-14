#include "ChannelSmsSend.hpp"
#include "../../Utils.hpp"





ChannelSmsSend::ChannelSmsSend(Connection & aConnection):
	Super(aConnection)
{
}





void ChannelSmsSend::sendSms(const QString & aRecipient, const QString & aText)
{
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
	qDebug() << "TODO";

	return {};
}





void ChannelSmsSend::processIncomingMessage(const QByteArray & aMessage)
{
	// TODO
}
