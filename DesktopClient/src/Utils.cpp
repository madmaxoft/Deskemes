#include "Utils.hpp"
#include <cassert>





namespace Utils
{





QByteArray toHex(const QByteArray & aData)
{
	static const char hexChars[] = "0123456789abcdef";
	QByteArray res;
	res.reserve(aData.size() * 2);
	for (const auto & ch: aData)
	{
		res.push_back(hexChars[(ch >> 4) & 0x0f]);
		res.push_back(hexChars[ch & 0x0f]);
	}
	return res;
}





void writeBE16(QByteArray & aDest, quint16 aValue)
{
	aDest.push_back(static_cast<char>((aValue >> 8) & 0xff));
	aDest.push_back(static_cast<char>(aValue        & 0xff));
}





QByteArray writeBE16(quint16 aValue)
{
	QByteArray res;
	writeBE16(res, aValue);
	return res;
}





void writeBE32(QByteArray & aDest, quint32 aValue)
{
	aDest.push_back(static_cast<char>((aValue >> 24) & 0xff));
	aDest.push_back(static_cast<char>((aValue >> 16) & 0xff));
	aDest.push_back(static_cast<char>((aValue >> 8)  & 0xff));
	aDest.push_back(static_cast<char>(aValue         & 0xff));
}





QByteArray writeBE32(quint32 aValue)
{
	QByteArray res;
	writeBE32(res, aValue);
	return res;
}





void writeBE16Lstring(QByteArray & aDest, const QByteArray & aValue)
{
	auto len = aValue.length();
	assert(len < 65536);
	writeBE16(aDest, static_cast<quint16>(len));
	aDest.append(aValue);
}





quint16 readBE16(const QByteArray & aData, int aIndex)
{
	assert(aData.size() >= aIndex + 1);
	return (
		static_cast<quint16>(static_cast<quint8>(aData[aIndex]) << 8) |
		static_cast<quint16>(static_cast<quint8>(aData[aIndex + 1]))
			);
}

}  // namespace Utils
