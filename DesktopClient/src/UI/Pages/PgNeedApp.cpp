#include "PgNeedApp.hpp"
#include "ui_PgNeedApp.h"
#include "../NewDeviceWizard.hpp"
#include "../../Comm/AdbAppInstaller.hpp"
#include "../../Comm/AdbCommunicator.hpp"





/// The URL that is opened in the device's browser in order to install the app
#define APK_URL "http://www.github.com/madmaxoft/Deskemes/releases"





PgNeedApp::PgNeedApp(ComponentCollection & aComponents, NewDeviceWizard & aParent) :
	Super(nullptr),
	mComponents(aComponents),
	mParent(aParent),
	mUI(new Ui::PgNeedApp)
{
	mUI->setupUi(this);

	connect(mUI->btnPushOverUsb, &QPushButton::pressed, this, &PgNeedApp::tryInstallApp);
	connect(mUI->btnOpenBrowser, &QPushButton::pressed, this, &PgNeedApp::openBrowserWithAppWebPage);
}





PgNeedApp::~PgNeedApp()
{
	// Nothing explicit needed yet
}





void PgNeedApp::initializePage()
{
	// Set the UI up:
	mUI->btnPushOverUsb->show();
	mUI->pbAppInstall->hide();
	mUI->lblAppInstallResult->hide();

	// Set up notification for when the device changes state:
	auto dd = mComponents.get<DetectedDevices>();
	connect(dd.get(), &DetectedDevices::deviceStatusChanged, this, &PgNeedApp::checkDeviceStatus);
}





bool PgNeedApp::isComplete() const
{
	switch (mParent.device()->status())
	{
		case DetectedDevices::Device::dsBlacklisted:  return true;
		case DetectedDevices::Device::dsNeedApp:      return false;
		case DetectedDevices::Device::dsNeedPairing:  return true;
		case DetectedDevices::Device::dsNoPubKey:     return true;
		case DetectedDevices::Device::dsOffline:      return false;
		case DetectedDevices::Device::dsOnline:       return true;
		case DetectedDevices::Device::dsUnauthorized: return true;
		case DetectedDevices::Device::dsFailed:       return true;
	}
	#ifdef _MSC_VER
		assert(!"Unknown device status");
		throw LogicError("Unknown device status: %1", mParent.device()->status());
	#endif
	return true;
}





int PgNeedApp::nextId() const
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





void PgNeedApp::checkDeviceStatus()
{
	// TODO
}





void PgNeedApp::tryInstallApp()
{
	auto appInst = new AdbAppInstaller;
	connect(appInst, &AdbAppInstaller::installed,     this, &PgNeedApp::onAppInstalled);
	connect(appInst, &AdbAppInstaller::errorOccurred, this, &PgNeedApp::onAppInstallFailed);
	appInst->autoDeleteSelf();
	appInst->installAppFromFile(mParent.device()->enumeratorDeviceID(), "deskemes.apk");
	mUI->btnPushOverUsb->hide();
	mUI->pbAppInstall->show();
	mUI->lblAppInstallResult->hide();
}





void PgNeedApp::openBrowserWithAppWebPage()
{
	auto shellCmd = QString::fromUtf8("am start -a android.intent.action.VIEW -d %1").arg(APK_URL);
	auto adbComm = new AdbCommunicator;
	connect(adbComm, &AdbCommunicator::connected,      this,    [=](){ adbComm->assignDevice(mParent.device()->enumeratorDeviceID()); });
	connect(adbComm, &AdbCommunicator::deviceAssigned, this,    [=](){ adbComm->shellExecuteV1(shellCmd.toUtf8()); });
	connect(adbComm, &AdbCommunicator::disconnected,   adbComm, &AdbCommunicator::deleteLater);
	connect(adbComm, &AdbCommunicator::error,          adbComm, &AdbCommunicator::deleteLater);
	adbComm->start();
}





void PgNeedApp::onAppInstalled()
{
	mUI->btnPushOverUsb->show();
	mUI->lblAppInstallResult->setText(tr("App installation succeeded."));
	mUI->lblAppInstallResult->show();
	mUI->pbAppInstall->hide();
}





void PgNeedApp::onAppInstallFailed(QString aErrorDesc)
{
	mUI->btnPushOverUsb->show();
	mUI->lblAppInstallResult->setText(tr("App installation failed:\n%1").arg(aErrorDesc));
	mUI->lblAppInstallResult->show();
	mUI->pbAppInstall->hide();
}
