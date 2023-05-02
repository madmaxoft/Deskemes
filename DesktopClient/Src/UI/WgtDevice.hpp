#pragma once

#include <memory>
#include <QWidget>





// fwd:
class Device;
namespace Ui
{
	class WgtDevice;
}





/** Displays the UI for a single device. */
class WgtDevice:
	public QWidget
{
	using Super = QWidget;
	Q_OBJECT


public:

	explicit WgtDevice(std::shared_ptr<Device> aDevice, QWidget * aParent = nullptr);

	~WgtDevice();


private:

	/** The Qt-managed UI. */
	std::unique_ptr<Ui::WgtDevice> mUI;

	/** The device displayed in this widget. */
	std::shared_ptr<Device> mDevice;


protected slots:

	/** Updates the identification labels - IMEI, IMSI, carrier. */
	void updateIdentification(const QString & aImei, const QString & aImsi, const QString & aCarrierName);

	/** Updates the battery level indicator. */
	void updateBattery(const double aBatteryLevelPercent);

	/** Updates the signal sterngth indicator. */
	void updateSignalStrength(const double aSignalStrengthPercent);
};
