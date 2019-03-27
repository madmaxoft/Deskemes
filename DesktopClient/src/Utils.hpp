#pragma once

#include <QByteArray>





namespace Utils
{





/** Converts the input data into hex representation (without any prefix, lowercase). */
QByteArray toHex(const QByteArray & aData);

/** Writes to aDest the two-byte number (MSB first). */
void writeBE16(QByteArray & aDest, quint16 aValue);

/** Returns the number encoded as big-endian two bytes. */
QByteArray writeBE16(quint16 aValue);

/** Writes to aDest the four-byte number (MSB first). */
void writeBE32(QByteArray & aDest, quint32 aValue);

/** Returns the number encoded as big-endian four bytes. */
QByteArray writeBE32(quint32 aValue);

/** Writes to aDest the two-byte length (MSB first) and then aValue. */
void writeBE16Lstring(QByteArray & aDest, const QByteArray & aValue);

/** Reads 2 bytes out of aBytes starting at the specified index and returns the big-endian value they represent. */
quint16 readBE16(const QByteArray & aData, int aIndex = 0);





/** Reads 2 bytes out of aBytes and returns the big-endian value they represent. */
constexpr quint16 readBE16(const quint8 * aBytes)
{
	return (
		static_cast<quint16>(aBytes[0] << 8) |
		static_cast<quint16>(aBytes[1])
	);
}





/** Reads 4 bytes out of aBytes and returns the big-endian value they represent. */
constexpr quint32 readBE32(const quint8 * aBytes)
{
	return (
		static_cast<quint32>(aBytes[0] << 24) |
		static_cast<quint32>(aBytes[1] << 16) |
		static_cast<quint32>(aBytes[2] << 8) |
		static_cast<quint32>(aBytes[3])
	);
}




}  // namespace Utils


/** Returns the first 4 bytes out of aChars interpreted as a 32-bit big-endian number.
Checks that exactly 4 characters are supplied. */
constexpr quint32 operator ""_4cc(const char * aChars, const size_t aNumChars)
{
	return (
		(aNumChars != 4) ? (throw std::runtime_error("Need exactly 4 characters")) :
		static_cast<quint32>(static_cast<quint8>(aChars[0]) << 24) |
		static_cast<quint32>(static_cast<quint8>(aChars[1]) << 16) |
		static_cast<quint32>(static_cast<quint8>(aChars[2]) << 8) |
		static_cast<quint32>(static_cast<quint8>(aChars[3]))
	);
}
