#pragma once

#include <memory>
#include <QTcpSocket>
#include <QImage>
#include "../Exception.hpp"





/** Implements the ADB communication protocol and handling.
Connects to the localhost ADB server and issues command to it according to the functions called.
All operations are performed asynchronously.
Note that each operation takes exclusive ownership of the connection and it is then no longer possible to
request another operation. You need to create a new instance in order to request a different operation. */
class AdbCommunicator:
	public QObject
{
	using Super = QObject;

	Q_OBJECT


public:

	/** Creates a new instance of a communicator. */
	AdbCommunicator(QObject * aParent = nullptr);


public Q_SLOTS:

	/** Starts connecting to the local ADB server.
	A connected() signal is emitted once connected, and ready() signal is then emitted once the connection
	is verified to be to a local ADB server. The error() signal is emitted on error. */
	void start();

	/** Asks ADB for the list of devices.
	The devices are reported back using the updateDeviceList() signal.
	ADB closes the connection after sending the whole device list. */
	void listDevices();

	/** Sends the "host:track-devices" request to ADB and goes into device tracking mode.
	In this mode, ADB sends the list of devices periodically, it is parsed and reported by emitting updateDeviceList().
	The communicator needs to be connected first (csReady).
	Note that some ADB versions don't correctly report the device going from dsNeedAuth to dsOnline. */
	void trackDevices();

	/** Instructs the ADB server to switch the connection to the specified device.
	All further communications on this connection will be relayed directly to the ADB daemon on the device.
	Once the switch is confirmed by the ADB server, the deviceAssigned() signal is emitted. */
	void assignDevice(const QByteArray & aDeviceID);

	/** Instructs the device to send a screenshot.
	Needs a device assigned first. Multiple successive screenshots can be requested using one object instance.
	Once the screenshot is received, the screenshotReceived() signal is emitted.
	If the screenshotting fails, the regular error() signal is emitted. */
	void takeScreenshot();

	/** Sets up port reversing - a TCP server on the device, forwarding incoming connections to the local machine.
	Needs a device assigned first.
	Once the device confirms the port-reversing, the portReversingEstalished() signal is emitted.
	If the device responds with an error, the regular error() signal is emitted. */
	void portReverse(
		quint16 aDevicePort,
		quint16 aLocalPort
	);


	/** Executes the specified command through the device's shell, the original V1 protocol.
	Needs a device assigned first.
	The shell command's stdout and stderr are delivered back in the shellIncomingOutput() signal, mixed together (no protocol support for split delivery).
	Once the shell command terminates, the device closes the comm, causing the disconnected() signal to be emitted.
	If the device responds with an error, the regular error() signal is emitted. */
	void shellExecuteV1(const QByteArray & aCommand);

	/** Tries to load the contents of the ADB pub key file (~/.android/adbkey.pub).
	Returns the loaded un-base64-ed key on success, empty string on failure. */
	static QByteArray getAdbPubKey();


Q_SIGNALS:

	/** Emitted once the socket conntects to the ADB server port. */
	void connected();

	/** Emitted once the socket is disconnected, either by the local ADB server, or by us. */
	void disconnected();

	/** Emitted whenever an error occurs, either while connecting, or while communicating. */
	void error(const QString & aErrorText);

	/** Emitted whenever the ADB sends an updated device list after "host:track-devices".
	The OnlineDeviceIDs lists devices that are online and may be communicated with.
	The UnauthDeviceIDs lists devices that requires ADB authentication before communication.
	The OtherDeviceIDs lists devices that are known but unavailable (ADB "offline", "bootloader" etc.) */
	void updateDeviceList(
		const QList<QByteArray> & aOnlineDeviceIDs,
		const QList<QByteArray> & aUnauthDeviceIDs,
		const QList<QByteArray> & aOtherDeviceIDs
	);

	/** Emitted after the ADB server confirms assigning a device to this connection (see assignDevice()). */
	void deviceAssigned(const QByteArray & aDeviceID);

	/** Emitted after a new screenshot is received from the device after takeScreenshot(). */
	void screenshotReceived(const QByteArray & aDeviceID, const QImage & aImage);

	/** Emitted after the device confirms port reversing.
	The connection is then terminated from the device side. */
	void portReversingEstablished(const QByteArray & aDeviceID);

	/** Emitted after receiving StdOut or StdErr data from the device while in ShellV1 mode (no protocol support for split delivery).
	May be emitted multiple times when more data is received. */
	void shellIncomingData(const QByteArray & aDeviceID, const QByteArray & aStdOutOrErr);


protected:

	/** The communicator state. */
	enum EState
	{
		csCreated,     ///< Freshly created, not connecting yet.
		csConnecting,  ///< Waiting for connection to the local ADB server.
		csReady,       ///< Connected to the local ADB server, ready for any command.
		csBroken,      ///< An irrecoverable error has been detected on the socket, communication cannot continue.

		csListingDevicesStart,  ///< after listDevices() has been called, waiting for the OKAY response
		csListingDevices,       ///< after listDevices() has been called, waiting for the device list.

		csTrackingDevicesStart,  ///< after trackDevices() has been called, waiting for the OKAY response.
		csTrackingDevices,       ///< after trackDevices() has been called, tracking device list changes.

		csAssignDevice,    ///< after assignDevice() has been called, waiting for the OKAY response.
		csDeviceAssigned,  ///< after assignDevice has succeeded

		csScreenshottingStart,  ///< after takeScreenshot() has been called, waiting for the framebuffer geometry
		csScreenshotting,       ///< after takeScreenshot() has been called, waiting for the framebuffer contents.

		csPortReversing,  ///< after portReverse() has been called

		csExecutingShellV1,  ///< after shellExecuteV1() has been called, executing a shell command and relaying stdout + stderr
	};

	/** The TCP socket to the ADB server, used for communication. */
	QTcpSocket mSocket;

	/** Buffer for the incoming data until it is parsed into a full packet. */
	QByteArray mIncomingData;

	/** The current state of the connection. */
	EState mState;

	/** The device to which this connection is assigned, if any (see assignDevice()). */
	QByteArray mAssignedDeviceID;

	// Dimensions of the framebuffer, when screenshotting (see takeScreenshot()):
	uint32_t mFramebufferDepth;   // bits-per-pixel
	uint32_t mFramebufferSize;    // bytes
	uint32_t mFramebufferWidth;   // pixels
	uint32_t mFramebufferHeight;  // pixels


	/** Writes the hex4-formatted length and then the message to the connection. */
	void writeHex4(const QByteArray & aMessage);

	/** Parses the device list (as received from "host:track-devices")
	Emits updateDeviceList() accordingly. */
	void parseDeviceList(const QByteArray & aMessage);

	/** Looks into mIncomingData if there's an OKAY or FAIL<len><reason> response in there.
	If so, removes it from mIncomingData and in case of failure, emits the error() signal.
	Returns true if an OKAY was extracted, false otherwise. */
	bool extractOkayOrFail();

	/** Looks into mIncomingData if there's an entire hex4-lengthed packet available.
	Returns a composite value, the first field specifies if the packet was found,
	and if so, the packet itself is returned as the second field. */
	std::pair<bool, QByteArray> extractHex4Packet();

	/** Decodes the image received in mIncomingData and removes its data from the buffer.
	Emits the screenshotReceived() signal with the received image. */
	void processScreenshot();


protected Q_SLOTS:

	/** Emitted by mSocket when it succeeds connecting. */
	void onSocketConnected();

	/** Emitted by mSocket when it encounters an error. */
	void onSocketError(QAbstractSocket::SocketError aError);

	/** Emitted by mSocket when it disconnects from the remote endpoint. */
	void onSocketDisconnected();

	/** Emitted by mSocket when there's incoming data available for reading. */
	void onSocketReadyRead();
};
