#include "InfoChannel.hpp"
#include <cassert>
#include "../../Utils.hpp"





/** The values' datatypes used in the protocol. */
enum
{
	vdtInt8    = 0x01,
	vdtInt16   = 0x02,
	vdtInt32   = 0x03,
	vdtInt64   = 0x04,
	vdtFixed32 = 0x05,
	vdtFixed64 = 0x06,
	vdtString  = 0x07,
};





InfoChannel::InfoChannel(Connection & aConnection):
	Super(aConnection)
{
}





void InfoChannel::queryAll()
{
	assert(mIsOpen);
	sendMessage("????");
}





void InfoChannel::queryBattery()
{
	assert(mIsOpen);
	sendMessage("batl");
}





void InfoChannel::querySignal()
{
	assert(mIsOpen);
	sendMessage("sist");
}





void InfoChannel::queryIdentification()
{
	assert(mIsOpen);
	sendMessage("carrimeiimsi");
}





void InfoChannel::queryCurrentTime()
{
	assert(mIsOpen);
	sendMessage("time");
}





QVariant InfoChannel::value(const quint32 aKey) const
{
	QMutexLocker lock(&mMtx);
	auto itr = mValues.find(aKey);
	if (itr == mValues.end())
	{
		return {};
	}
	return itr->second;
}





double InfoChannel::batteryLevel() const
{
	return value("batl"_4cc).toDouble();
}





double InfoChannel::signalStrength() const
{
	return value("sigs"_4cc).toDouble();
}





QString InfoChannel::carrierName() const
{
	return QString::fromUtf8(value("carr"_4cc).toByteArray());
}





QString InfoChannel::imei() const
{
	return QString::fromUtf8(value("imei"_4cc).toByteArray());
}





QString InfoChannel::imsi() const
{
	return QString::fromUtf8(value("imsi"_4cc).toByteArray());
}





QDateTime InfoChannel::currentTime() const
{
	auto val = value("time"_4cc);
	if (!val.isValid())
	{
		return {};
	}
	return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(val.toDouble() * 1000));
}





void InfoChannel::setValue(const quint32 aKey, QVariant && aValue)
{
	{
		QMutexLocker lock(&mMtx);
		mValues[aKey] = aValue;
	}
	emit valueChanged(aKey, aValue);

	// Emit signals for known values:
	switch (aKey)
	{
		case "batl"_4cc: emit receivedBattery(aValue.toDouble()); break;
		case "sist"_4cc: emit receivedSignalStrength(aValue.toDouble()); break;
		case "carr"_4cc: emit receivedCarrierName(QString::fromUtf8(aValue.toByteArray())); break;
		case "imei"_4cc: emit receivedImei(QString::fromUtf8(aValue.toByteArray())); break;
		case "imsi"_4cc: emit receivedImsi(QString::fromUtf8(aValue.toByteArray())); break;
		case "time"_4cc: emit receivedTime(QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(aValue.toDouble() * 1000))); break;
	}
}






#define NEED_BYTES(N) \
	if (i + N > len) \
	{ \
		qDebug() << "Value has been cut off, need " << N << " bytes, got " << len - i; \
		return; \
	}

void InfoChannel::processIncomingMessage(const QByteArray & aMessage)
{
	auto len = aMessage.size();
	for (int i = 0; i < len;)
	{
		if (i + 6 >= len)
		{
			// Not enough bytes for the smallest possible value
			return;
		}
		auto valueKey = Utils::readBE32(aMessage, i);
		auto dataType = aMessage[i + 4];
		i += 5;
		switch (dataType)
		{
			case vdtInt8:
			{
				setValue(valueKey, static_cast<qint8>(aMessage[i]));
				i += 1;
				break;
			}
			case vdtInt16:
			{
				NEED_BYTES(2);
				setValue(valueKey, static_cast<qint16>(Utils::readBE16(aMessage, i)));
				i += 2;
				break;
			}
			case vdtInt32:
			{
				NEED_BYTES(4);
				setValue(valueKey, static_cast<qint32>(Utils::readBE32(aMessage, i)));
				i += 4;
				break;
			}
			case vdtInt64:
			{
				NEED_BYTES(8);
				setValue(valueKey, static_cast<qint64>(Utils::readBE64(aMessage, i)));
				i += 8;
				break;
			}
			case vdtFixed32:
			{
				NEED_BYTES(4);
				setValue(valueKey, static_cast<double>(static_cast<qint32>(Utils::readBE32(aMessage, i))) / 65536.0);
				i += 4;
				break;
			}
			case vdtFixed64:
			{
				NEED_BYTES(8);
				setValue(valueKey, static_cast<double>(static_cast<qint64>(Utils::readBE64(aMessage, i))) / (65536.0 * 65536.0));
				i += 8;
				break;
			}
			case vdtString:
			{
				NEED_BYTES(2);
				auto stringLen = Utils::readBE16(aMessage, i);
				if (i + stringLen + 2 > len)
				{
					qDebug() << "String value cut off, need " << stringLen << " bytes, got " << len - i;
					return;
				}
				setValue(valueKey, aMessage.mid(i + 2, stringLen));
				i += 2 + stringLen;
				break;
			}
			default:
			{
				// We cannot continue parsing this message, we don't know how large the value is
				qWarning() << "Unknown value datatype: " << static_cast<int>(aMessage[i + 4]);
				return;
			}
		}
	}
}
