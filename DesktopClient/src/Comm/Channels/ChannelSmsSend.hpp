#pragma once

#include "../Connection.hpp"





/** Implements the `sms.send` channel protocol.
Provides methods for message sending and dividing synchronously. */
class ChannelSmsSend:
	public Connection::Channel
{
	using Super = Connection::Channel;

	Q_OBJECT


public:

	/** An exception that is thrown when dividing a message fails. */
	class DivisionError: public Exception
	{
	};



	ChannelSmsSend(Connection & aConnection);

	/** Sends the specified message to the recipient.
	Any reported errors are silently ignored. */
	void sendSms(const QString & aRecipient, const QString & aText);

	/** Divides the specified message into parts that can be each sent using one SMS.
	Throws a DivisionError on error. */
	std::vector<QString> divideMessage(const QString & aText);


private:

	// Channel override:
	void processIncomingMessage(const QByteArray & aMessage) override;
};
