#include "Logger.hpp"

#include <QDateTime>





Logger::Logger(const QString & aFileName):
	mLogFile(aFileName)
{
	if (!mLogFile.open(QFile::WriteOnly | QFile::Append))
	{
		throw std::runtime_error("Cannot open log file for appending");
	}
	mLogFile.write(QString("\n\n%1\tLogfile opened\n").arg(currentTimestamp()).toUtf8());
}





QString Logger::currentTimestamp()
{
	auto now = QDateTime::currentDateTimeUtc();
	return now.toString("yyyy-MM-dd hh:mm:ss.zzz");
}





void Logger::logInternal(const QByteArray & aLogData)
{
	QMutexLocker lock(&mMtxLogFile);
	mLogFile.write(currentTimestamp().toUtf8());
	mLogFile.write("\t", 1);
	mLogFile.write(aLogData);
	mLogFile.write("\n", 1);
}





void Logger::logHexInternal(const QByteArray & aHexData, const QByteArray & aLogData)
{
	QMutexLocker lock(&mMtxLogFile);
	mLogFile.write(currentTimestamp().toUtf8());
	mLogFile.write("\t", 1);
	mLogFile.write(aLogData);
	mLogFile.write("\n", 1);
	const int bytesPerLine = 32;
	char buffer[bytesPerLine * 4 + 2];
	memset(buffer, ' ', sizeof(buffer));
	const int brk = bytesPerLine * 3 + 1;
	buffer[0] = '\t';
	buffer[brk - 1] = '\t';
	buffer[sizeof(buffer) - 1] = '\n';
	for (int idx = 0, len = aHexData.length(); idx < len; idx += bytesPerLine)
	{
		for (int chIdx = 0; chIdx < bytesPerLine; ++chIdx)
		{
			if (idx + chIdx < len)
			{
				auto ch = aHexData[idx + chIdx];
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
