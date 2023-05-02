#include "UdpBroadcaster.hpp"
#include <cassert>
#include <QNetworkInterface>
#include <QUdpSocket>
#include "../Utils.hpp"
#include "../InstallConfiguration.hpp"
#include "TcpListener.hpp"





UdpBroadcaster::UdpBroadcaster(ComponentCollection & aComponents, QObject * aParent):
	Super(aParent),
	ComponentSuper(aComponents),
	mPrimaryPort(0),
	mAltPort(0),
	mIsDiscovery(false)
{
	connect(&mTimer, &QTimer::timeout, this, &UdpBroadcaster::broadcastBeacon);
}





void UdpBroadcaster::start(quint16 aPrimaryPort, quint16 aAltPort)
{
	assert(mPrimaryPort == 0);  // Already started
	assert(aPrimaryPort != 0);
	mPrimaryPort = aPrimaryPort;
	mAltPort = aAltPort;
	mTimer.start(1000);
}





void UdpBroadcaster::startDiscovery()
{
	mIsDiscovery = true;
}





void UdpBroadcaster::endDiscovery()
{
	mIsDiscovery = false;
}





QByteArray UdpBroadcaster::createBeacon(bool aIsDiscovery)
{
	auto ic = mComponents.get<InstallConfiguration>();
	auto listener = mComponents.get<TcpListener>();
	QByteArray beacon;
	beacon.append("Deskemes");
	beacon.append('\0');  // version, MSB
	beacon.append('\1');  // version, LSB
	Utils::writeBE16Lstring(beacon, ic->publicID());
	Utils::writeBE16(beacon, listener->listeningPort());
	beacon.append(aIsDiscovery ? '\1' : '\0');
	return beacon;
}





void UdpBroadcaster::broadcastBeacon()
{
	static const auto beaconRegular = createBeacon(false);
	static const auto beaconDiscovery = createBeacon(true);
	const auto & beacon = mIsDiscovery ? beaconDiscovery : beaconRegular;

	// Broadcast the beacon on all interfaces / addresses:
	for (const auto & addr: QNetworkInterface::allAddresses())
	{
		QUdpSocket socket;
		socket.bind(addr);
		socket.writeDatagram(beacon, QHostAddress::Broadcast, mPrimaryPort);
		if (mAltPort > 0)
		{
			socket.writeDatagram(beacon, QHostAddress::Broadcast, mAltPort);
		}
	}
}
