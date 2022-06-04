#pragma once

#include <QString>
#include <QByteArray>
#include <QMutex>
#include <QFile>
#include "StringFormatter.hpp"





/** A logger that writes its output to a single file.
The log-writing can be called simultaneously from multiple threads.
Note that there's a MultiLogger class / component managing multiple instances of this class.
The logger forces a flush on the log file after every FLUSH_AFTER_N_MESSAGES number of messages written,
and always after a hex dump. */
class Logger
{
	friend class PrefixLogger;  // Needs access to logInternal() and logHexInternal()

protected:

	/** After writing this many messages, the log file is flushed. */
	static const int FLUSH_AFTER_N_MESSAGES = 4;


	/** The file where the log data is actually written. */
	QFile mLogFile;

	/** The mutex protecting mLogFile from multithreaded access. */
	QMutex mMtxLogFile;

	/** Number of log messages to be yet written until a flush is forced on the log file. */
	int mNumMessagesUntilFlush;


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

	/** Flushes the log file.
	Can be called from any thread, is thread-safe also in regard to all the logging functions.
	Called periodically by MultiLogger. */
	void flush();
};





/** Relays log messages to a Logger, but every message is prefixed with a constant string, given to the constructor.
Typically, the prefix is something like "Module: ", which is used when multiple modules share the same Logger. */
class PrefixLogger
{

	/** The Logger where the messages are output. */
	Logger & mLogger;

	/** The prefix to use for log messages. */
	const QByteArray mPrefix;


public:

	/** Creates a new instance that binds to the specified Logger instance and uses the specified prefix for messages. */
	PrefixLogger(Logger & aLogger, const QString & aPrefix):
		mLogger(aLogger),
		mPrefix(aPrefix.toUtf8())
	{
	}

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
		mLogger.logInternal(mPrefix + StringFormatter::format(aFormatString, aArgs...).toUtf8());
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
		return mLogger.logHexInternal(aData, mPrefix + StringFormatter::format(aFormatString, aArgs...).toUtf8());
	}
};
