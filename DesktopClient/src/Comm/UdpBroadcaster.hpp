#pragma once

#include <atomic>
#include <QThread>
#include <QUdpSocket>
#include <QTimer>
#include "../ComponentCollection.hpp"





/** Broadcasts the UDP beacons so that devices on the local network know about this Deskemes instance.
Normally broadcasts a non-discovery beacon; upon request it broadcasts several discovery beacons before returning
back to broadcasting non-discovery ones. */
class UdpBroadcaster:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckUdpBroadcaster>
{
	using Super = QObject;
	Q_OBJECT


public:

	/** The default UDP port on which the devices listen. */
	static const quint16 DEFAULT_BROADCASTER_PORT = 24816;

	/** The alternate UDP port on which the devices listen if their default port is not available. */
	static const quint16 ALTERNATE_BROADCASTER_PORT = 4816;

	explicit UdpBroadcaster(ComponentCollection & aComponents, QObject * aParent = nullptr);

	virtual ~UdpBroadcaster() override {}

	/** Starts broadcasting the beacon on the two specified UDP ports.
	If aAltPort is zero, only the aPrimaryPort is used. */
	void start(
		quint16 aPrimaryPort = DEFAULT_BROADCASTER_PORT,
		quint16 aAltPort = ALTERNATE_BROADCASTER_PORT
	);

	/** Turns the Discovery flag on for several upcoming broadcasts. */
	void broadcastDiscoveryBeacon();


protected:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The timer used for periodicity. */
	QTimer mTimer;

	/** The primary port on which the broadcasts should be made. 0 if not started. */
	quint16 mPrimaryPort;

	/** The secondary port on which the broadcasts should be made. 0 if not used. */
	quint16 mAltPort;

	/** Number of times that a discovery beacon should be sent instead of a regular beacon.
	While positive, the periodic beacon is broadcast with its Discovery flag on, and this number is decreased. */
	std::atomic<int> mNumDiscoveryBroadcasts;


	/** Returns the data that should be sent as the beacon.
	The aIsDiscovery flag is sent in the beacon; discovery indicates that the user is actively searching for new
	devices via Deskemes' Add new device wizard. */
	QByteArray createBeacon(bool aIsDiscovery);


protected slots:

	/** Broadcasts a single beacon through mSocket. */
	void broadcastBeacon();
};
