#pragma once





#include <QAbstractTableModel>

#include "../Comm/DetectedDevices.hpp"





/** Provides a Model for the detected devices, so that they can be displayed in the UI.
The model automatically binds to the ComponentCollection::ckDetectedDevices. */
class DetectedDevicesModel:
	public QAbstractTableModel
{
	using Super = QAbstractTableModel;

	Q_OBJECT


public:

	/** The columns provided by this model. */
	enum
	{
		colDeviceName,  ///< The device's Name, or EnumeratorID if name not available
		colStatus,      ///< The status of the device
		colAvatar,      ///< An avatar representing the device

		colMax,
	};

	enum
	{
		roleDevPtr = Qt::UserRole + 1,  ///< The role used to query the underlying DevicePtr for the row
	};


	/** Creates a new instance of the class, bound to the specified cc's ckDetectedDevices. */
	explicit DetectedDevicesModel(ComponentCollection & aComponents, QObject * aParent = nullptr);

	virtual ~DetectedDevicesModel();

	// QAbstractTableModel overrides:
	virtual int rowCount(const QModelIndex & aParent) const override;
	virtual int columnCount(const QModelIndex & aParent) const override;
	virtual QVariant data(const QModelIndex & aIndex, int aRole) const override;
	virtual QVariant headerData(int aSection, Qt::Orientation aOrientation, int aRole) const override;


protected:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** The currently present detected devices, in a non-specified order.
	This list is modified by the DetectedDevice's callbacks (onDeviceAdded(), onDeviceRemoved() etc.). */
	std::vector<DetectedDevices::DevicePtr> mDevices;

	/** Returns the index in mDevices of the specified device.
	Throws an Exception if the device is not present in the list. */
	int indexOfDevice(DetectedDevices::DevicePtr aDevice);


protected Q_SLOTS:

	/** Called just after a new device is added. */
	void addDevice(DetectedDevices::DevicePtr aDevice);

	/** Called just before a device is removed. */
	void removeDevice(DetectedDevices::DevicePtr aDevice);

	/** Called when a device's status changes. */
	void updateDeviceStatus(DetectedDevices::DevicePtr aDevice);

	/** Called when a device's avatar changes. */
	void updateDeviceAvatar(DetectedDevices::DevicePtr aDevice);

	/** Called when a device's name changes. */
	void updateDeviceName(DetectedDevices::DevicePtr aDevice);
};

