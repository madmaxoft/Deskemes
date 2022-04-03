#include "Device.hpp"
#include <cassert>
#include "Comm/Channels/InfoChannel.hpp"
#include "MultiLogger.hpp"





Device::Device(ComponentCollection & aComponents, const QByteArray & aDeviceID):
	mDeviceID(aDeviceID),
	mLogger(createLogger(aComponents, aDeviceID))
{
	assert(!mDeviceID.isEmpty());
}





void Device::addConnection(ConnectionPtr aConnection)
{
	mLogger.log("Adding connection %1, transport %2; friendlyName %3",
		aConnection->connectionID(), aConnection->transportKind(),
		aConnection->friendlyName().valueOrDefault()
	);
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
	mLogger.log("Opening an info channel...");
	if (mConnections.empty())
	{
		mLogger.log("WARNING: Cannot open InfoChannel, there's no connection");
		return;
	}

	auto conn = mConnections[0];
	mInfoChannel = std::make_shared<InfoChannel>(*conn);
	connect(mInfoChannel.get(), &InfoChannel::receivedImei,           this, &Device::receivedIdentification);
	connect(mInfoChannel.get(), &InfoChannel::receivedImsi,           this, &Device::receivedIdentification);
	connect(mInfoChannel.get(), &InfoChannel::receivedCarrierName,    this, &Device::receivedIdentification);
	connect(mInfoChannel.get(), &InfoChannel::receivedBattery,        this, &Device::batteryUpdated);
	connect(mInfoChannel.get(), &InfoChannel::receivedSignalStrength, this, &Device::signalStrengthUpdated);
	connect(mInfoChannel.get(), &InfoChannel::opened, this,
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
	connect(mInfoChannel.get(), &InfoChannel::failed, this,
		[this](Connection::Channel * aChannel, const quint16 aErrorCode, const QByteArray & aErrorMsg)
		{
			Q_UNUSED(aChannel);
			mInfoChannel.reset();
			qWarning() << "Failed to open the info channel: " << aErrorCode << ": " << aErrorMsg;
		}
	);
	connect(conn.get(), &Connection::disconnected, this,
		[this]()
		{
			mInfoChannel.reset();
		}
	);
	if (!conn->openChannel(mInfoChannel, "info"))
	{
		mLogger.log("WARNING: Failed to open the InfoChannel.");
	}
}





Logger & Device::createLogger(ComponentCollection & aComponents, const QString & aDeviceID)
{
	return aComponents.get<MultiLogger>()->logger("Device-" + aDeviceID);
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
		// Either the channel is waiting for open confirmation, or its connection has been closed.
		// TODO: If this happens 3 times in a row, close the channel and open a new one.
		mLogger.log("Info channel is not open.");
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
	mLogger.log("Connection %1 (transport %2) disconnected",
		aConnection->connectionID(), aConnection->transportKind()
	);

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
		mLogger.log("This was the last connection, going offline.");
		mInfoQueryTimer.stop();
		emit goingOffline();
	}
}
