#pragma once

#include <QMutex>
#include <QFile>

#include "ComponentCollection.hpp"
#include "Logger.hpp"





/** Manages multiple loggers by-device and by-subsystem. */
class MultiLogger:
	public ComponentCollection::Component<ComponentCollection::ckMultiLogger>
{
	using Super = ComponentCollection::Component<ComponentCollection::ckMultiLogger>;


public:

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

