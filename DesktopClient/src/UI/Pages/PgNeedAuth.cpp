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
	}
}





void PgNeedAuth::checkDeviceStatus(DetectedDevices::DevicePtr aDevice)
{
	// Check the device state whenever *any* device's state changes, so ignore the param:
	Q_UNUSED(aDevice);

	// Auto-advance the wizard once the device advances in state:
	emit completeChanged();
	switch (mParent.device()->status())
	{
		case DetectedDevices::Device::dsNeedApp:
		case DetectedDevices::Device::dsNeedPairing:
		case DetectedDevices::Device::dsOnline:
		{
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
	mUI->lblRsaKey->setText(
		tr("This computer's RSA key fingerprint is:\n%1 (MD5)\n  -- or --\n%2 (SHA256)")
		.arg(md5, sha256)
	);
}
