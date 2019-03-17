#include "Utils.hpp"
#include <cassert>





namespace Utils
{





void Utils::writeBE16(QByteArray & aDest, quint16 aValue)
{
	aDest.push_back(static_cast<char>(aValue / 256));
	aDest.push_back(static_cast<char>(aValue % 256));
}





void Utils::writeBE16Lstring(QByteArray & aDest, const QByteArray & aValue)
{
	auto len = aValue.length();
	assert(len < 65536);
	writeBE16(aDest, static_cast<quint16>(len));
	aDest.append(aValue);
}





}  // namespace Utils
