#include "MultiLogger.hpp"

#include <QDir>
#include <QDateTime>





MultiLogger::MultiLogger(ComponentCollection & aComponents, const QString & aLogsFolder):
	Super(aComponents),
	mLogsFolder(aLogsFolder)
{
	QDir dir;
	dir.mkpath(aLogsFolder);
}





Logger & MultiLogger::logger(const QString & aLoggerName)
{
	QMutexLocker locker(&mMtxLoggers);
	auto itr = mLoggers.find(aLoggerName);
	if (itr != mLoggers.end())
	{
		return *(itr->second.get());
	}
	auto res = mLoggers.insert({aLoggerName, std::make_unique<Logger>(loggerFileName(aLoggerName))});
	return *(res.first->second.get());
}





QString MultiLogger::loggerFileName(QString aLoggerName)
{
	// Sanitize the logger name, at least somewhat:
	static const QString illegal("/\\\"\':;&%*?|<>");
	for (auto & ch: aLoggerName)
	{
		if ((ch.unicode() >= 0) && (ch.unicode() < 32))
		{
			ch = '_';
		}
		else if (illegal.contains(ch))
		{
			ch = '_';
		}
	}

	auto res = mLogsFolder + "/" + aLoggerName + ".log";
	return res;
}
