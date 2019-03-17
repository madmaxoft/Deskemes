#pragma once

#include <QByteArray>





namespace Utils
{





/** Writes to aDest the two-byte LE number. */
void writeBE16(QByteArray & aDest, quint16 aValue);

/** Writes to aDest the two-byte LE length and then aValue. */
void writeBE16Lstring(QByteArray & aDest, const QByteArray & aValue);





}  // namespace Utils
