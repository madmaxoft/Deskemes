#include "PgPairConfirm.hpp"
#include <cassert>
#include <QBitmap>
#include "ui_PgPairConfirm.h"
#include "../../../lib/PolarSSL-cpp/Sha1Checksum.h"
#include "../../DB/DevicePairings.hpp"
#include "../../Utils.hpp"
#include "../../InstallConfiguration.hpp"
#include "../NewDeviceWizard.hpp"





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

	setThumbprint();
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





void PgPairConfirm::setThumbprint()
{
	static const int wid = 17;
	static const int hei = 9;
	static const uint32_t palette[] =
	{
		0xffffffff,
		0xff0000ff, 0xff00ff00, 0xffff0000,
		0xff7f7f7f, 0xffcfcfcf,
		0xff00ffff, 0xffffff00, 0xffff00ff,
		0xff00007f, 0xff007f00, 0xff7f0000,
		0xff00ff7f, 0xffff7f00, 0xff7f00ff,
		0xff007fff, 0xff7fff00, 0xffff007f,
		0xff007f7f, 0xff7f7f00, 0xff7f007f,
	};

	Sha1Checksum md;
	auto conn = mParent.connection();
	const auto & remotePublicID = conn->remotePublicID().value();
	const auto & remotePublicKey = conn->remotePublicKeyData().value();
	const auto & localPublicID = mComponents.get<InstallConfiguration>()->publicID();
	auto pairings = mComponents.get<DevicePairings>();
	auto pairing = pairings->lookupDevice(remotePublicID);
	const auto & localPublicKey = pairing.value().mLocalPublicKeyData;
	md.update(reinterpret_cast<const Byte *>(localPublicID.constData()), static_cast<size_t>(localPublicID.size()));
	md.update(reinterpret_cast<const Byte *>(remotePublicID.constData()), static_cast<size_t>(remotePublicID.size()));
	md.update(reinterpret_cast<const Byte *>(localPublicKey.constData()), static_cast<size_t>(localPublicKey.size()));
	md.update(reinterpret_cast<const Byte *>(remotePublicKey.constData()), static_cast<size_t>(remotePublicKey.size()));
	Sha1Checksum::Checksum hash;
	md.finalize(hash);

	// The drunken bishop algorithm: http://www.dirk-loss.de/sshvis/drunken_bishop.pdf
	// Adapted from OpenSSH C source: https://cvsweb.openbsd.org/cgi-bin/cvsweb/src/usr.bin/ssh/Attic/key.c?rev=1.70
	uint32_t * colors = new uint32_t[wid * hei];
	memset(colors, 0, wid * hei * sizeof(*colors));
	int x = wid / 2;
	int y = hei / 2;
	for (Byte h: hash)
	{
		int val = h;
		for (Byte shift = 0; shift < 4; ++shift)
		{
			x += ((val & 0x01) != 0) ? 1 : -1;
			y += ((val & 0x02) != 0) ? 1 : -1;
			x = Utils::clamp(x, 0, wid - 1);
			y = Utils::clamp(y, 0, hei - 1);
			val = val >> 2;
			colors[x + wid * y] += 1;
		}
	}
	for (int i = 0; i < wid * hei; ++i)
	{
		colors[i] = palette[Utils::clamp<uint32_t>(colors[i], 0, sizeof(palette) / sizeof(*palette) - 1)];
	}
	QImage img(reinterpret_cast<const uchar *>(colors), wid, hei, QImage::Format_RGB32);
	mUI->imgThumbprint->setPixmap(QPixmap::fromImage(img));
}
