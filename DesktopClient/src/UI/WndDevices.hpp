#pragma once

#include <memory>
#include <map>
#include <string>
#include <QMainWindow>
#include <QScrollArea>
#include <QBoxLayout>
#include "../Device.hpp"





// fwd:
class WgtDevice;
class ComponentCollection;
namespace Ui
{
	class WndDevices;
}





/** The main app window that shows all the known devices. */
class WndDevices:
	public QMainWindow
{
	Q_OBJECT
	using Super = QMainWindow;

public:


	explicit WndDevices(ComponentCollection & aComponents, QWidget * aParent = nullptr);

	~WndDevices();


private:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::WndDevices> mUI;

	/** The scroll area that displays all the devices' infowidgets. */
	QScrollArea * mDevicesScroller;

	/** The container for all the devices' infowidgets inside mDevicesScroller. */
	QWidget * mDevicesContainer;

	/** The layout for all the devices' infowidgets inside mDevicesContainer. */
	QBoxLayout * mDevicesLayout;

	/** The infowidget for each known device, indexed by DeviceID. */
	std::map<QByteArray, std::unique_ptr<WgtDevice>> mDeviceWidgets;


	/** Adds the UI for the specified device. */
	void addDeviceUI(DevicePtr aDevice);

	/** Removes the UI for the specified device. */
	void delDeviceUI(DevicePtr aDevice);


private slots:

	/** Called when a new device is added. */
	void onDeviceAdded(DevicePtr aDevice);

	/** Called when a device is removed. */
	void onDeviceRemoved(DevicePtr aDevice);

	/** Opens the New device wizard.
	Called when the user selects New device from the menu. */
	void addNewDevice();
};
