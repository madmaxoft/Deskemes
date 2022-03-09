#pragma once

#include <QMutex>
#include <QFile>

#include "ComponentCollection.hpp"





/** Implements a logger that handles multiple log-files by-device and by-subsystem. */
class MultiLogger:
	public ComponentCollection::Component<ComponentCollection::ckMultiLogger>
{
	using Super = ComponentCollection::Component<ComponentCollection::ckMultiLogger>;


public:

	/** A logger that writes its output to a single file.
	The log-writing can be called simultaneously from multiple threads. */
	class Logger
	{
	protected:

		/** The file where the log data is actually written. */
		QFile mLogFile;

		/** The mutex protecting mLogFile from multithreaded access. */
		QMutex mMtxLogFile;


		/** Returns the current timestamp as a string, to be prepended to each log line. */
		static QString currentTimestamp();


	public:

		/** Creates an instance that writes to the specified file. */
		Logger(const QString & aFileName);

		/** Writes the specified text to the log file, prepending the timestamp to it.
		Can be called from multiple threads concurrently.
		Does the actual logging work, since it has the raw text data and size. */
		void log(const char * aText, int aTextSize);

		/** Writes the specified text to the log file, prepending the timestamp to it.
		Can be called from multiple threads concurrently.
		Overload that handles the log("direct text") case efficiently. */
		template <size_t N>
		void log(const char (&aText)[N])
		{
			return log(aText, static_cast<int>(N));
		}

		/** Writes the specified text to the log file, prepending the timestamp to it.
		Can be called from multiple threads concurrently. */
		void log(const QByteArray & aText)
		{
			return log(aText.data(), aText.size());
		}

		/** Writes the specified text to the log file, prepending the timestamp to it.
		Can be called from multiple threads concurrently.
		Used mainly for formatted output. */
		void log(const QString & aText)
		{
			return log(aText.toUtf8());
		}

		/** Logs the label, followed by a formatted hex dump of the data.
		Does the actual logging, since it has the raw label data and size. */
		void logHex(const QByteArray & aData, const char * aLabel, int aLabelSize);

		/** Logs the label, followed by a formatted hex dump of the data.
		Overload that handles the logHex(..., "direct label") case efficiently. */
		template <size_t N>
		void logHex(const QByteArray & aData, const char (&aLabel)[N])
		{
			return logHex(aData, aLabel, static_cast<int>(N));
		}

		/** Logs the label, followed by a formatted hex dump of the data. */
		void logHex(const QByteArray & aData, const QByteArray & aLabel)
		{
			return logHex(aData, aLabel.data(), aLabel.size());
		}

		/** Logs the label, followed by a formatted hex dump of the data.
		Used mainly for formatted-label output. */
		void logHex(const QByteArray & aData, const QString & aLabel)
		{
			return logHex(aData, aLabel.toUtf8());
		}
	};



	/** Creates the MultiLogger that stores its log files in the specified folder. */
	MultiLogger(ComponentCollection & aComponents, const QString & aLogsFolder);

	/** Returns the main logger. */
	Logger & mainLogger() { return logger("main"); }

	/** Returns the logger for the specified name (device / subsystem).
	If there's no such logger yet, creates one and starts its logfile. */
	Logger & logger(const QString & aLoggerName);


protected:

	/** The folder where to store the log files. */
	QString mLogsFolder;

	/** All the loggers currently known.
	Protected against multithreaded access by mMtxLoggers. */
	std::map<QString, std::unique_ptr<Logger>> mLoggers;

	/** Protects mLoggers against multithreaded access. */
	QMutex mMtxLoggers;


	/** Returns the name of the file to which the specified logger should write. */
	QString loggerFileName(QString aLoggerName);
};

