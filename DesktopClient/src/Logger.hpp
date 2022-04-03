#pragma once

#include <QString>
#include <QByteArray>
#include <QMutex>
#include <QFile>
#include "StringFormatter.hpp"





/** A logger that writes its output to a single file.
The log-writing can be called simultaneously from multiple threads.
Note that there's a MultiLogger class / component managing multiple instances of this class. */
class Logger
{
protected:

	/** The file where the log data is actually written. */
	QFile mLogFile;

	/** The mutex protecting mLogFile from multithreaded access. */
	QMutex mMtxLogFile;


	/** Returns the current timestamp as a string, to be prepended to each log line. */
	static QString currentTimestamp();

	/** Writes the specified log data into the output file, pre-pending it with the current timestamp. */
	void logInternal(const QByteArray & aLogData);

	/** Writes the specified log data along with a formatted hex dump into the output file,
	pre-pending it with the current timestamp. */
	void logHexInternal(const QByteArray & aHexData, const QByteArray & aLogData);


public:

	/** Creates an instance that writes to the specified file. */
	Logger(const QString & aFileName);

	/** Writes a formatted string to the log.
	The format string follows QString::arg()'s formatting, aArgs can be anything serializable by QDebug. */
	template <size_t N, typename... T>
	void log(const char (&aFormatString)[N], const T &... aArgs)
	{
		log(QString::fromUtf8(aFormatString, N), aArgs...);
	}

	/** Writes a formatted string to the log.
	The format string follows QString::arg()'s formatting, aArgs can be anything serializable by QDebug. */
	template <typename... T>
	void log(const QString & aFormatString, const T &... aArgs)
	{
		logInternal(StringFormatter::format(aFormatString, aArgs...).toUtf8());
	}

	/** Logs the formatted label, followed by a hex dump of the data.
	The format string follows QString::arg()'s formatting, aArgs can be anything serializable by QDebug. */
	template <size_t N, typename... T>
	void logHex(const QByteArray & aData, const char (&aFormatString)[N], const T &... aArgs)
	{
		logHex(aData, QString::fromUtf8(aFormatString, N), aArgs...);
	}

	/** Logs the formatted label, followed by a hex dump of the data.
	The format string follows QString::arg()'s formatting, aArgs can be anything serializable by QDebug. */
	template <typename... T>
	void logHex(const QByteArray & aData, const QString & aFormatString, const T... aArgs)
	{
		return logHexInternal(aData, StringFormatter::format(aFormatString, aArgs...).toUtf8());
	}
};
