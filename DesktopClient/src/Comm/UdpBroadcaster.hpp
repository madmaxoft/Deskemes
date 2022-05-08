#pragma once

#include <atomic>
#include <QThread>
#include <QUdpSocket>
#include <QTimer>
#include "../ComponentCollection.hpp"





/** Broadcasts the UDP beacons so that devices on the local network know about this Deskemes instance.
Normally broadcasts a non-discovery beacon; after startDiscovery() is called it broadcasts discovery beacons
until endDiscovery() is called. */
class UdpBroadcaster:
	public QObject,
	public ComponentCollection::Component<ComponentCollection::ckUdpBroadcaster>
{
	using Super = QObject;
	using ComponentSuper = ComponentCollection::Component<ComponentCollection::ckUdpBroadcaster>;

	Q_OBJECT


public:

	/** The default UDP port on which the devices listen. */
	static const quint16 DEFAULT_BROADCASTER_PORT = 24816;

	/** The alternate UDP port on which the devices listen if their default port is not available. */
	static const quint16 ALTERNATE_BROADCASTER_PORT = 4816;

	explicit UdpBroadcaster(ComponentCollection & aComponents, QObject * aParent = nullptr);

	virtual ~UdpBroadcaster() override {}

	// ComponentCollection::Component override:
	virtual void start() override;

	/** Turns the Discovery flag on. */
	void startDiscovery();

	/** Turns the Discovery flag off. */
	void endDiscovery();


protected:

	/** The timer used for periodicity. */
	QTimer mTimer;

	/** The primary port on which the broadcasts should be made. 0 if not started. */
	quint16 mPrimaryPort;

	/** The secondary port on which the broadcasts should be made. 0 if not used. */
	quint16 mAltPort;

	/** True if discovery beacons should be sent instead of regular beacons.
	Manipulated by startDiscovery() and endDiscovery(). */
	bool mIsDiscovery;

	/** The logger for all log mesages produced by this component. */
	Logger & mLogger;


	/** Starts broadcasting the beacon on the two specified UDP ports.
	If aAltPort is zero, only the aPrimaryPort is used. */
	void startBroadcasting(
		quint16 aPrimaryPort = DEFAULT_BROADCASTER_PORT,
		quint16 aAltPort = ALTERNATE_BROADCASTER_PORT
	);

	/** Returns the data that should be sent as the beacon.
	The aIsDiscovery flag is sent in the beacon; discovery indicates that the user is actively searching for new
	devices via Deskemes' Add new device wizard. */
	QByteArray createBeacon(bool aIsDiscovery);


protected slots:

	/** Broadcasts a single beacon through mSocket. */
	void broadcastBeacon();
};
