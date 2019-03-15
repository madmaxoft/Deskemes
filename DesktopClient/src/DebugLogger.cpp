#include "DebugLogger.hpp"
#include <QDebug>





/** The previous message handler. */
static QtMessageHandler g_OldHandler;





DebugLogger::DebugLogger()
{
	g_OldHandler = qInstallMessageHandler(messageHandler);
}





DebugLogger & DebugLogger::get()
{
	static DebugLogger inst;
	return inst;
}





std::vector<DebugLogger::Message> DebugLogger::lastMessages() const
{
	QMutexLocker lock(&mMtx);
	std::vector<Message> res;
	size_t max = sizeof(mMessages) / sizeof(mMessages[0]);
	res.reserve(max);
	for (size_t i = 0; i < max; ++i)
	{
		size_t idx = (i + mNextMessageIdx) % max;
		if (mMessages[idx].mDateTime.isValid())
		{
			res.push_back(mMessages[idx]);
		}
	}
	return res;
}





void DebugLogger::messageHandler(
	QtMsgType a_Type,
	const QMessageLogContext & a_Context,
	const QString & a_Message
)
{
	DebugLogger::get().addMessage(a_Type, a_Context, a_Message);
	g_OldHandler(a_Type, a_Context, a_Message);
}





void DebugLogger::addMessage(
	QtMsgType a_Type,
	const QMessageLogContext & a_Context,
	const QString & a_Message
)
{
	QMutexLocker lock(&mMtx);
	mMessages[mNextMessageIdx] = Message(a_Type, a_Context, a_Message);
	mNextMessageIdx = (mNextMessageIdx + 1) % (sizeof(mMessages) / sizeof(mMessages[0]));
}
