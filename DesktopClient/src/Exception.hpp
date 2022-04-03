#pragma once

#include <stdexcept>

#include <QString>
#include <QDebug>

#include "Logger.hpp"





/** A class that is the base of all exceptions used in the program.
Provides parametrical description message formatting and optionally, logging.
Usage:
throw Exception(logger, formatString, argValue1, argValue2);
throw Exception(formatString, argValue1, argValue2);
*/
class Exception:
	public std::runtime_error
{
public:

	/** Creates an exception with the specified values formatted (as in QString.arg()) as the description.
	Logs the exception text to the specified logger. */
	template <typename... OtherTs>
	Exception(Logger & aLogger, const QString & aFormatString, const OtherTs &... aArgValues):
		std::runtime_error(formatAndLog(aLogger, aFormatString, aArgValues...).toStdString())
	{
	}


	/** Creates an exception with the specified values formatted (as in QString.arg()) as the description. */
	template <typename... OtherTs>
	Exception(const QString & aFormatString, const OtherTs &... aArgValues):
		std::runtime_error(StringFormatter::format(aFormatString, aArgValues...).toStdString())
	{
	}


protected:


	/** Formats the message, sends it to the specified logger and returns it. */
	template <typename... OtherTs>
	QString formatAndLog(Logger & aLogger, const QString & aFormatString, const OtherTs &... aValues)
	{
		auto res = StringFormatter::format(aFormatString, aValues...);
		aLogger.log(res);
		return res;
	}
};





/** Descendant to be used for errors in the runtime. */
class RuntimeError: public Exception
{
public:
	using Exception::Exception;
};





/** Descendant to be used for errors that indicate a bug in the program. */
class LogicError: public Exception
{
public:
	using Exception::Exception;
};
