#pragma once

#include <QString>
#include <QDebug>





/** Allow sending (an Utf-8-encoded) std::string directly to QDebug. */
inline QDebug operator << (QDebug aDebug, const std::string & aStr)
{
	return (aDebug << QString::fromStdString(aStr));
}





/** Provides functions for formatting string using QString::arg(),
but the arguments are stringified using QDebug.
Up to 10 arguments can be passed.
This enables us to output many more custom types very simply. */
namespace StringFormatter
{





/** Simple wrapper over QDebug that requires an output string and sets the underlying QDebug to nospace, noquote. */
class Debug:
	public QDebug
{
	using Super = QDebug;


public:

	Debug(QString * aOutput):
		Super(aOutput)
	{
		nospace();
		noquote();
	}
};





inline QString format(const QString & aFormatString)
{
	return aFormatString;
}





/** Returns the format string formatted with 1 argument. */
template <
	typename ArgType1
>
inline QString format(
	const QString & aFormatString,
	const ArgType1 & aArg1
)
{
	QString arg1;
	Debug(&arg1) << aArg1;
	return aFormatString.arg(arg1);
}





/** Returns the format string formatted with 2 arguments. */
template <
	typename ArgType1,
	typename ArgType2
>
inline QString format(
	const QString & aFormatString,
	const ArgType1 & aArg1,
	const ArgType2 & aArg2
)
{
	QString arg1, arg2;
	Debug(&arg1) << aArg1;
	Debug(&arg2) << aArg2;
	return aFormatString.arg(arg1, arg2);
}





/** Returns the format string formatted with 3 arguments. */
template <
	typename ArgType1,
	typename ArgType2,
	typename ArgType3
>
inline QString format(
	const QString & aFormatString,
	const ArgType1 & aArg1,
	const ArgType2 & aArg2,
	const ArgType3 & aArg3
)
{
	QString arg1, arg2, arg3;
	Debug(&arg1) << aArg1;
	Debug(&arg2) << aArg2;
	Debug(&arg3) << aArg3;
	return aFormatString.arg(arg1, arg2, arg3);
}




/** Returns the format string formatted with 4 arguments. */
template <
	typename ArgType1,
	typename ArgType2,
	typename ArgType3,
	typename ArgType4
>
inline QString format(
	const QString & aFormatString,
	const ArgType1 & aArg1,
	const ArgType2 & aArg2,
	const ArgType3 & aArg3,
	const ArgType4 & aArg4
)
{
	QString arg1, arg2, arg3, arg4;
	Debug(&arg1) << aArg1;
	Debug(&arg2) << aArg2;
	Debug(&arg3) << aArg3;
	Debug(&arg4) << aArg4;
	return aFormatString.arg(arg1, arg2, arg3, arg4);
}




/** Returns the format string formatted with 5 arguments. */
template <
	typename ArgType1,
	typename ArgType2,
	typename ArgType3,
	typename ArgType4,
	typename ArgType5
>
inline QString format(
	const QString & aFormatString,
	const ArgType1 & aArg1,
	const ArgType2 & aArg2,
	const ArgType3 & aArg3,
	const ArgType4 & aArg4,
	const ArgType5 & aArg5
)
{
	QString arg1, arg2, arg3, arg4, arg5;
	Debug(&arg1) << aArg1;
	Debug(&arg2) << aArg2;
	Debug(&arg3) << aArg3;
	Debug(&arg4) << aArg4;
	Debug(&arg5) << aArg5;
	return aFormatString.arg(arg1, arg2, arg3, arg4, arg5);
}




/** Returns the format string formatted with 6 arguments. */
template <
	typename ArgType1,
	typename ArgType2,
	typename ArgType3,
	typename ArgType4,
	typename ArgType5,
	typename ArgType6
>
inline QString format(
	const QString & aFormatString,
	const ArgType1 & aArg1,
	const ArgType2 & aArg2,
	const ArgType3 & aArg3,
	const ArgType4 & aArg4,
	const ArgType5 & aArg5,
	const ArgType6 & aArg6
)
{
	QString arg1, arg2, arg3, arg4, arg5, arg6;
	Debug(&arg1) << aArg1;
	Debug(&arg2) << aArg2;
	Debug(&arg3) << aArg3;
	Debug(&arg4) << aArg4;
	Debug(&arg5) << aArg5;
	Debug(&arg6) << aArg6;
	return aFormatString.arg(arg1, arg2, arg3, arg4, arg5, arg6);
}




/** Returns the format string formatted with 7 arguments. */
template <
	typename ArgType1,
	typename ArgType2,
	typename ArgType3,
	typename ArgType4,
	typename ArgType5,
	typename ArgType6,
	typename ArgType7
>
inline QString format(
	const QString & aFormatString,
	const ArgType1 & aArg1,
	const ArgType2 & aArg2,
	const ArgType3 & aArg3,
	const ArgType4 & aArg4,
	const ArgType5 & aArg5,
	const ArgType6 & aArg6,
	const ArgType7 & aArg7
)
{
	QString arg1, arg2, arg3, arg4, arg5, arg6, arg7;
	Debug(&arg1) << aArg1;
	Debug(&arg2) << aArg2;
	Debug(&arg3) << aArg3;
	Debug(&arg4) << aArg4;
	Debug(&arg5) << aArg5;
	Debug(&arg6) << aArg6;
	Debug(&arg7) << aArg7;
	return aFormatString.arg(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}




/** Returns the format string formatted with 8 arguments. */
template <
	typename ArgType1,
	typename ArgType2,
	typename ArgType3,
	typename ArgType4,
	typename ArgType5,
	typename ArgType6,
	typename ArgType7,
	typename ArgType8
>
inline QString format(
	const QString & aFormatString,
	const ArgType1 & aArg1,
	const ArgType2 & aArg2,
	const ArgType3 & aArg3,
	const ArgType4 & aArg4,
	const ArgType5 & aArg5,
	const ArgType6 & aArg6,
	const ArgType7 & aArg7,
	const ArgType8 & aArg8
)
{
	QString arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8;
	Debug(&arg1) << aArg1;
	Debug(&arg2) << aArg2;
	Debug(&arg3) << aArg3;
	Debug(&arg4) << aArg4;
	Debug(&arg5) << aArg5;
	Debug(&arg6) << aArg6;
	Debug(&arg7) << aArg7;
	Debug(&arg8) << aArg8;
	return aFormatString.arg(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
}




/** Returns the format string formatted with 9 arguments. */
template <
	typename ArgType1,
	typename ArgType2,
	typename ArgType3,
	typename ArgType4,
	typename ArgType5,
	typename ArgType6,
	typename ArgType7,
	typename ArgType8,
	typename ArgType9
>
inline QString format(
	const QString & aFormatString,
	const ArgType1 & aArg1,
	const ArgType2 & aArg2,
	const ArgType3 & aArg3,
	const ArgType4 & aArg4,
	const ArgType5 & aArg5,
	const ArgType6 & aArg6,
	const ArgType7 & aArg7,
	const ArgType8 & aArg8,
	const ArgType9 & aArg9
)
{
	QString arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9;
	Debug(&arg1) << aArg1;
	Debug(&arg2) << aArg2;
	Debug(&arg3) << aArg3;
	Debug(&arg4) << aArg4;
	Debug(&arg5) << aArg5;
	Debug(&arg6) << aArg6;
	Debug(&arg7) << aArg7;
	Debug(&arg8) << aArg8;
	Debug(&arg9) << aArg9;
	return aFormatString.arg(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
}




/** Returns the format string formatted with 10 arguments. */
template <
	typename ArgType1,
	typename ArgType2,
	typename ArgType3,
	typename ArgType4,
	typename ArgType5,
	typename ArgType6,
	typename ArgType7,
	typename ArgType8,
	typename ArgType9,
	typename ArgType10
>
inline QString format(
	const QString & aFormatString,
	const ArgType1 & aArg1,
	const ArgType2 & aArg2,
	const ArgType3 & aArg3,
	const ArgType4 & aArg4,
	const ArgType5 & aArg5,
	const ArgType6 & aArg6,
	const ArgType7 & aArg7,
	const ArgType8 & aArg8,
	const ArgType9 & aArg9,
	const ArgType10 & aArg10
)
{
	QString arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10;
	Debug(&arg1) << aArg1;
	Debug(&arg2) << aArg2;
	Debug(&arg3) << aArg3;
	Debug(&arg4) << aArg4;
	Debug(&arg5) << aArg5;
	Debug(&arg6) << aArg6;
	Debug(&arg7) << aArg7;
	Debug(&arg8) << aArg8;
	Debug(&arg9) << aArg9;
	Debug(&arg10) << aArg10;
	return aFormatString.arg(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
}

}  // namespace StringFormatter
