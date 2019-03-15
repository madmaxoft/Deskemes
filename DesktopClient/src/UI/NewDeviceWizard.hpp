#pragma once

#include <QWizard>
#include "../ComponentCollection.hpp"





class NewDeviceWizard:
	public QWizard
{
	using Super = QWizard;
	Q_OBJECT


public:

	/** The page IDs of the individual pages. */
	enum
	{
		pgConnectionType,
		pgTcpDeviceList,
		pgBluetoothDeviceList,
		pgUsbDeviceList,
	};


	/** Creates a new wizard with the specified parent. */
	NewDeviceWizard(ComponentCollection & aComponents, QWidget * aParent);


protected:

	/** The components of the entire app. */
	ComponentCollection & mComponents;
};
