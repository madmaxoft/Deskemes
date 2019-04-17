#include "Device.hpp"
#include <cassert>
#include "Comm/Channels/InfoChannel.hpp"





Device::Device(const QByteArray & aDeviceID):
	mDeviceID(aDeviceID)
{
	assert(!mDeviceID.isEmpty());
}





void Device::addConnection(ConnectionPtr aConnection)
{
	mConnections.push_back(aConnection);
	connect(aConnection.get(), &Connection::disconnected, this, &Device::connDisconnected);

	// If this is the first connection added, start querying basic information from the device periodically:
	if (mConnections.size() == 1)
	{
		connect(&mInfoQueryTimer, &QTimer::timeout, this, &Device::queryStatusInfo);
		mInfoQueryTimer.start(1000);
	}

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
		[this](Connection::Channel * aChannel, const quint16 aErrorCode, const QByteArray & aErrorMsg)
		{
			Q_UNUSED(aChannel);
			mInfoChannel.reset();
			qWarning() << "Failed to open the info channel: " << aErrorCode << ": " << aErrorMsg;
		}
	);
	connect(conn.get(), &Connection::disconnected,
		[this]()
		{
			mInfoChannel.reset();
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





void Device::connDisconnected(Connection * aConnection)
{
	// Remove the connection from mConnections:
	mConnections.erase(std::remove_if(mConnections.begin(), mConnections.end(),
		[aConnection](ConnectionPtr aStoredConnection)
		{
			return (aConnection == aStoredConnection.get());
		}),
		mConnections.end()
	);

	// If this was the last connection, go offline:
	if (mConnections.empty())
	{
		mInfoQueryTimer.stop();
		emit goingOffline();
	}
}
