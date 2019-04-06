#include "PgPairConfirm.hpp"
#include <cassert>
#include "ui_PgPairConfirm.h"
#include "../NewDeviceWizard.hpp"
#include "../../DB/DevicePairings.hpp"





PgPairConfirm::PgPairConfirm(ComponentCollection & aComponents, NewDeviceWizard & aParent):
	Super(&aParent),
	mComponents(aComponents),
	mParent(aParent),
	mUI(new Ui::PgPairConfirm)
{
	mUI->setupUi(this);
	setTitle(tr("Confirm encryption key"));
	setSubTitle(tr("This device uses a key not yet encountered"));
	setCommitPage(true);
	connect(mUI->chbKeysMatch, &QCheckBox::stateChanged, this, &QWizardPage::completeChanged);
}





PgPairConfirm::~PgPairConfirm()
{
	// Nothing explicit needed
}





int PgPairConfirm::nextId() const
{
	return NewDeviceWizard::pgPairingInProgress;
}





bool PgPairConfirm::isComplete() const
{
	return mUI->chbKeysMatch->isChecked();
}





void PgPairConfirm::initializePage()
{
	auto conn = mParent.connection();
	assert(conn->remotePublicID().isPresent());
	assert(conn->remotePublicKeyData().isPresent());
	auto pairings = mComponents.get<DevicePairings>();
	const auto & id = conn->remotePublicID().value();
	auto pairing = pairings->lookupDevice(id);
	assert(pairing.isPresent());
	assert(!pairing.value().mLocalPublicKeyData.isEmpty());
}





void PgPairConfirm::cleanupPage()
{
	// Skip the pgPairInit progress page and go directly back to device list:
	QMetaObject::invokeMethod(wizard(), "back", Qt::QueuedConnection);
}





bool PgPairConfirm::validatePage()
{
	// The user clicked the Commit button, store the pairing:
	mParent.connection()->localPairingApproved();
	return true;
}
