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





MultiLogger::Logger & MultiLogger::logger(const QString & aLoggerName)
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

	return mLogsFolder + "/" + aLoggerName + ".log";
}





/////////////////////////////////////////////////////////////////////////////////////////////
// MultiLogger::Logger:

MultiLogger::Logger::Logger(const QString & aFileName):
	mLogFile(aFileName)
{
	if (!mLogFile.open(QFile::WriteOnly | QFile::Append))
	{
		throw std::runtime_error("Cannot open log file for appending");
	}
	mLogFile.write(QString("\n\n%1\tLogfile opened\n").arg(currentTimestamp()).toUtf8());
}





QString MultiLogger::Logger::currentTimestamp()
{
	auto now = QDateTime::currentDateTimeUtc();
	return now.toString("yyyy-MM-dd hh:mm:ss.zzz");
}





void MultiLogger::Logger::log(const char * aText, int aTextSize)
{
	QMutexLocker lock(&mMtxLogFile);
	mLogFile.write(currentTimestamp().toUtf8());
	mLogFile.write("\t", 1);
	mLogFile.write(aText, aTextSize);
	mLogFile.write("\n", 1);
}





void MultiLogger::Logger::logHex(const QByteArray & aData, const char * aLabel, int aLabelSize)
{
	QMutexLocker lock(&mMtxLogFile);
	mLogFile.write(currentTimestamp().toUtf8());
	mLogFile.write("\t", 1);
	mLogFile.write(aLabel, aLabelSize);
	mLogFile.write("\n", 1);
	const int bytesPerLine = 16;
	char buffer[bytesPerLine * 4 + 2];
	memset(buffer, ' ', sizeof(buffer));
	const int brk = bytesPerLine * 3 + 1;
	buffer[0] = '\t';
	buffer[brk - 1] = '\t';
	buffer[sizeof(buffer) - 1] = '\n';
	for (int idx = 0, len = aData.length(); idx < len; idx += bytesPerLine)
	{
		for (int chIdx = 0; chIdx < bytesPerLine; ++chIdx)
		{
			if (idx + chIdx < len)
			{
				auto ch = aData[idx + chIdx];
				static const char hexChar[] = "0123456789abcdef";
				buffer[chIdx * 3 + 1] = hexChar[ch / 16];
				buffer[chIdx * 3 + 2] = hexChar[ch % 16];
				buffer[brk + chIdx] = ((ch < 32) || (ch > 127)) ? '.' : ch;
			}
			else
			{
				buffer[chIdx * 3 + 1] = ' ';
				buffer[chIdx * 3 + 2] = ' ';
				buffer[brk + chIdx] = ' ';
			}
		}
		if (len - idx < bytesPerLine)
		{
			size_t cutOff = bytesPerLine - (len - idx);
			buffer[sizeof(buffer) - cutOff - 1] = '\n';
			mLogFile.write(buffer, sizeof(buffer) - cutOff);
		}
		else
		{
			mLogFile.write(buffer, sizeof(buffer));
		}
	}
}
