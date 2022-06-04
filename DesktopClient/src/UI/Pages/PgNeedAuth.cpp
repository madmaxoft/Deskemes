#include "PgNeedAuth.hpp"
#include <QCryptographicHash>
#include "ui_PgNeedAuth.h"
#include "../../Comm/AdbCommunicator.hpp"
#include "../NewDeviceWizard.hpp"





PgNeedAuth::PgNeedAuth(ComponentCollection & aComponents, NewDeviceWizard & aParent):
	Super(nullptr),
	mComponents(aComponents),
	mParent(aParent),
	mUI(new Ui::PgNeedAuth)
{
	mUI->setupUi(this);
}





PgNeedAuth::~PgNeedAuth()
{
	// Nothing explicit needed yet
}





void PgNeedAuth::initializePage()
{
	mParent.logger().log("Initializing PubKey display.");
	initPubKeyDisplay();

	// Set up notification for when the device changes state:
	auto dd = mComponents.get<DetectedDevices>();
	connect(dd.get(), &DetectedDevices::deviceStatusChanged, this, &PgNeedAuth::checkDeviceStatus);
}





bool PgNeedAuth::isComplete() const
{
	return (nextId() != NewDeviceWizard::pgNeedAuth);
}





int PgNeedAuth::nextId() const
{
	switch (mParent.device()->status())
	{
		case DetectedDevices::Device::dsBlacklisted:  return NewDeviceWizard::pgBlacklisted;
		case DetectedDevices::Device::dsNeedApp:      return NewDeviceWizard::pgNeedApp;
		case DetectedDevices::Device::dsNeedPairing:  return NewDeviceWizard::pgPairInit;
		case DetectedDevices::Device::dsNoPubKey:     return NewDeviceWizard::pgFailed;
		case DetectedDevices::Device::dsOffline:      return NewDeviceWizard::pgFailed;
		case DetectedDevices::Device::dsOnline:       return NewDeviceWizard::pgSucceeded;
		case DetectedDevices::Device::dsUnauthorized: return NewDeviceWizard::pgNeedAuth;
		case DetectedDevices::Device::dsFailed:       return NewDeviceWizard::pgFailed;
	}
	#ifdef _MSC_VER
		assert(!"Unknown device status");
		throw LogicError("Unknown device status: %1", mParent.device()->status());
	#endif
}





void PgNeedAuth::checkDeviceStatus(DetectedDevices::DevicePtr aDevice)
{
	mParent.logger().log("Device status changed for device %1.", aDevice->enumeratorDeviceID());

	// Check the device state whenever *any* device's state changes, so ignore the param:
	Q_UNUSED(aDevice);

	// Auto-advance the wizard once the device advances in state:
	emit completeChanged();
	switch (mParent.device()->status())
	{
		case DetectedDevices::Device::dsNeedApp:
		case DetectedDevices::Device::dsNeedPairing:
		case DetectedDevices::Device::dsOnline:
		case DetectedDevices::Device::dsFailed:
		{
			mParent.logger().log("The device is in a final state %1, advancing the wizard.",
				DetectedDevices::Device::statusToString(mParent.device()->status())
			);
			wizard()->next();
			break;
		}
		default:
		{
			// No auto-action, the user may need to disconnect and reconect the device to re-auth
			break;
		}
	}
}





void PgNeedAuth::initPubKeyDisplay()
{
	auto adbPubKey = AdbCommunicator::getAdbPubKey();
	if (adbPubKey.isEmpty())
	{
		return;
	}
	auto md5 = QString::fromUtf8(QCryptographicHash::hash(adbPubKey, QCryptographicHash::Md5).toHex(':'));
	auto sha256 = QString::fromUtf8(QCryptographicHash::hash(adbPubKey, QCryptographicHash::Sha256).toHex(':'));
	mParent.logger().log("This computer's ADB pub key: MD5: %1 ; SHA-256: %2", md5, sha256);
	mUI->lblRsaKey->setText(
		tr("This computer's RSA key fingerprint is:\n%1 (MD5)\n  -- or --\n%2 (SHA256)")
		.arg(md5, sha256)
	);
}
