#pragma once

#include <QWizard>
#include "../ComponentCollection.hpp"
#include "../Comm/Connection.hpp"
#include "../Comm/DetectedDevices.hpp"





/** The wizard for adding new devices.
This object is also used by the individual pages to directly store their values for sharing (such as
the Connection to use). */
class NewDeviceWizard:
	public QWizard
{
	using Super = QWizard;
	Q_OBJECT


public:

	/** The page IDs of the individual pages. */
	enum
	{
		pgDeviceList,
		pgBlacklisted,
		pgNeedAuth,           ///< The device needs ADB authorization
		pgNeedApp,            ///< The device is accessible through ADB but doesn't have the app installed
		pgPairInit,           ///< The device app responds, but requests pairing
		pgPairConfirm,
		pgPairingInProgress,
		pgFailed,
		pgSucceeded,
	};


	/** Creates a new wizard with the specified parent. */
	NewDeviceWizard(ComponentCollection & aComponents, QWidget * aParent);

	virtual ~NewDeviceWizard() override;

	/** Returns the device selected by the user, that the wizard should operate on. */
	DetectedDevices::DevicePtr device() const { return mDevice; }

	/** Returns the connection that the wizard should operate on.
	Returns nullptr if no connection chosen yet. */
	ConnectionPtr connection() const { return mConnection; }

	/** Stores the device to work on. */
	void setDevice(DetectedDevices::DevicePtr aDevice);

	/** Stores the connection to work on. */
	void setConnection(ConnectionPtr aConnection);


protected:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The device that has been selected by the user. */
	DetectedDevices::DevicePtr mDevice;

	/** The connection selected which the wizard should bring to a full complete device.
	Set by pgTcpDeviceList, (TODO USB and Bluetooth). */
	ConnectionPtr mConnection;
};
