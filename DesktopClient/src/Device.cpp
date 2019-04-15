#include "Device.hpp"
#include <cassert>
#include "Comm/Channels/InfoChannel.hpp"





Device::Device(ConnectionPtr aConnection):
	mDeviceID(aConnection->remotePublicID().value()),
	mConnections({aConnection})
{
	assert(!mDeviceID.isEmpty());

	// Start an info channel on the connection:
	openInfoChannel();

	// Query basic information from the device periodically:
	connect(&mInfoQueryTimer, &QTimer::timeout, this, &Device::queryStatusInfo);
	mInfoQueryTimer.start(1000);
}





void Device::addConnection(ConnectionPtr aConnection)
{
	mConnections.push_back(aConnection);
	emit connectionAdded(this->shared_from_this(), aConnection);
}





const QString & Device::friendlyName() const
{
	if (mConnections.empty())
	{
		static const QString empty;
		return empty;
	}
	return mConnections[0]->friendlyName().value();
}





void Device::openInfoChannel()
{
	qDebug() << "Opening an info channel";
	if (mConnections.empty())
	{
		qWarning() << "Cannot open InfoChannel, there's no connection";
		return;
	}

	auto conn = mConnections[0];
	mInfoChannel = std::make_shared<InfoChannel>(*conn);
	connect(mInfoChannel.get(), &InfoChannel::receivedImei,           this, &Device::receivedIdentification);
	connect(mInfoChannel.get(), &InfoChannel::receivedImsi,           this, &Device::receivedIdentification);
	connect(mInfoChannel.get(), &InfoChannel::receivedCarrierName,    this, &Device::receivedIdentification);
	connect(mInfoChannel.get(), &InfoChannel::receivedBattery,        this, &Device::batteryUpdated);
	connect(mInfoChannel.get(), &InfoChannel::receivedSignalStrength, this, &Device::signalStrengthUpdated);
	connect(mInfoChannel.get(), &InfoChannel::opened,
		[this](Connection::Channel * aChannel)
		{
			if (aChannel != mInfoChannel.get())
			{
				// Someone replaced the info channel in the meantime
				return;
			}
			mInfoChannel->queryIdentification();
			mInfoChannel->querySignal();
			mInfoChannel->queryBattery();
		}
	);
	connect(mInfoChannel.get(), &InfoChannel::failed,
		[](Connection::Channel * aChannel, const quint16 aErrorCode, const QByteArray & aErrorMsg)
		{
			Q_UNUSED(aChannel);
			qWarning() << "Failed to open the info channel: " << aErrorCode << ": " << aErrorMsg;
		}
	);
	if (!conn->openChannel(mInfoChannel, "info"))
	{
		qWarning() << "Failed to open the InfoChannel for device " << friendlyName();
	}
}





void Device::queryStatusInfo()
{
	if (mInfoChannel == nullptr)
	{
		// Start opening a new Info channel:
		openInfoChannel();
		return;
	}
	if (!mInfoChannel->isOpen())
	{
		// The channel is waiting for open confirmation, not yet
		// TODO: If this happens 3 times in a row, close the channel and open a new one.
		return;
	}
	mInfoChannel->querySignal();
	mInfoChannel->queryBattery();
}





void Device::receivedIdentification()
{
	emit identificationUpdated(mInfoChannel->imei(), mInfoChannel->imsi(), mInfoChannel->carrierName());
}
