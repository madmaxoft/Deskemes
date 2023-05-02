#pragma once

#include <memory>
#include <QWizardPage>





// fwd:
class ComponentCollection;
class NewDeviceWizard;
class Connection;
namespace Ui
{
	class PgPairInit;
}





/** The NewDevice wizard page that sends the public key to the device and waits until the device sends
its own public key. */
class PgPairInit:
	public QWizardPage
{
	using Super = QWizardPage;

	Q_OBJECT


public:

	explicit PgPairInit(ComponentCollection & aComponents, NewDeviceWizard & aParent);

	virtual ~PgPairInit() override;

	// QWizardPage overrides:
	virtual void initializePage() override;
	virtual int nextId() const override;
	virtual bool isComplete() const override;


private:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The entire NewDevice wizard that stores the progress. */
	NewDeviceWizard & mParent;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::PgPairInit> mUI;

	/** Set to true once the remote public key is received. */
	bool mHasReceivedRemotePublicKey;

	/** Set to true once the local keypair is created. */
	bool mIsLocalKeyPairCreated;


protected slots:

	/** The connection has received the device's public key.
	Pairing can continue. Steps the wizard forward. */
	void receivedPublicKey(Connection * aConnection);

	/** The local keypair has been created.
	Called from the background thread creating the keypair after it finishes. */
	void localKeyPairCreated();
};
