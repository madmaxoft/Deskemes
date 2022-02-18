#include "AdbCommunicator.hpp"
#include <cassert>
#include <limits>
#include <thread>
#include <QDir>
#include "../Utils.hpp"





/** The port on which the ADB server listens on a computer. */
#define ADB_LOCAL_PORT 5037





/** Returns the numerical value represented by the specified hex character.
Unknown characters are considered to be zero. */
static uint8_t hexValue(char aChar)
{
	switch (aChar)
	{
		case '0': return 0x00;
		case '1': return 0x01;
		case '2': return 0x02;
		case '3': return 0x03;
		case '4': return 0x04;
		case '5': return 0x05;
		case '6': return 0x06;
		case '7': return 0x07;
		case '8': return 0x08;
		case '9': return 0x09;
		case 'a': return 0x0a;
		case 'b': return 0x0b;
		case 'c': return 0x0c;
		case 'd': return 0x0d;
		case 'e': return 0x0e;
		case 'f': return 0x0f;
		case 'A': return 0x0a;
		case 'B': return 0x0b;
		case 'C': return 0x0c;
		case 'D': return 0x0d;
		case 'E': return 0x0e;
		case 'F': return 0x0f;
	}
	return 0;
}





/** Converts the hex4-encoded number (such as "000c") to the number it represents.
Characters that are invalid are considered to be zeroes. */
static uint16_t hex4ToNumber(const QByteArray & aHex4EncodedNumber)
{
	assert(aHex4EncodedNumber.size() == 4);
	uint16_t res = 0;
	for (int i = 0; i < 4; ++i)
	{
		auto val = hexValue(aHex4EncodedNumber[i]);
		res = res * 16 + val;
	}
	return res;
}





/** Returns the specified number as a hex4-encoded string. */
static QByteArray numberToHex4(uint16_t aNumber)
{
	static const char hexChars[] = "0123456789ABCDEF";
	QByteArray res;
	res.resize(4);
	for (int i = 0; i < 4; ++i)
	{
		auto v = aNumber % 16;
		aNumber = aNumber >> 4;
		res[3 - i] = hexChars[v];
	}
	return res;
}





/** Decodes the first four bytes of aData into a LE UInt32 value. */
static uint32_t decodeLEUInt32(const char * aData)
{
	auto d = reinterpret_cast<const unsigned char *>(aData);
	return
	(
		static_cast<uint32_t>(d[0] << 24) |
		static_cast<uint32_t>(d[1] << 16) |
		static_cast<uint32_t>(d[2] << 8) |
		static_cast<uint32_t>(d[3])
	);
}




/** Returns the contents of the ADB public key file.
Returns an empty QByteArray if the pub key file cannot be read. */
static QByteArray loadAdbPubKeyFile()
{
	try
	{
		return Utils::readWholeFile(QDir::home().absoluteFilePath(".android/adbkey.pub"));
	}
	catch (...)
	{
		return {};
	}
}





////////////////////////////////////////////////////////////////////////////////
// AdbCommunicator:

AdbCommunicator::AdbCommunicator(QObject * aParent):
	Super(aParent),
	mState(csCreated),
	mFramebufferSize(0)
{
}





void AdbCommunicator::start()
{
	if ((mState != csCreated) && (mState != csBroken))
	{
		return;
	}
	mState = csConnecting;
	connect(&mSocket, &QTcpSocket::errorOccurred, this, &AdbCommunicator::onSocketError);
	connect(&mSocket, &QTcpSocket::connected,     this, &AdbCommunicator::onSocketConnected);
	connect(&mSocket, &QTcpSocket::disconnected,  this, &AdbCommunicator::onSocketDisconnected);
	connect(&mSocket, &QTcpSocket::readyRead,     this, &AdbCommunicator::onSocketReadyRead);
	mSocket.connectToHost("localhost", ADB_LOCAL_PORT);
}





void AdbCommunicator::listDevices()
{
	assert(mState == csReady);
	mState = csListingDevicesStart;
	qDebug() << "Requesting device list";
	writeHex4("host:devices");
	/*
	Expected response:
	OKAY
	<len><ID>\t<status>\n<ID>\t<status>...
	(socket close)
	*/
}





void AdbCommunicator::trackDevices()
{
	assert(mState == csReady);
	mState = csTrackingDevicesStart;
	qDebug() << "Requesting device list";
	writeHex4("host:track-devices");
	/*
	Expected response:
	OKAY
	<len><ID>\t<status>\n<ID>\t<status>...

	( device list changes )

	<len><ID>\t<status>\n...
	*/
}





void AdbCommunicator::assignDevice(const QByteArray & aDeviceID)
{
	switch (mState)
	{
		case csReady:
		{
			mState = csAssignDevice;
			writeHex4("host:transport:" + aDeviceID);
			mAssignedDeviceID = aDeviceID;
			break;
		}
		default:
		{
			assert(!"Invalid state");
			break;
		}
	}
}





void AdbCommunicator::takeScreenshot()
{
	switch (mState)
	{
		case csDeviceAssigned:
		{
			// Start the screenshotting:
			mState = csScreenshottingStart;
			writeHex4("framebuffer");
			break;
		}

		case csScreenshottingStart:
		{
			// We're already waiting for a screenshot, no need to request a new one.
			break;
		}

		case csScreenshotting:
		{
			// We're already screenshotting, request another one
			mSocket.write("s");
			break;
		}

		default:
		{
			assert("!Invalid state");
			break;
		}
	}
}





void AdbCommunicator::portReverse(quint16 aDevicePort, quint16 aLocalPort)
{
	qDebug() << "Setting up port-reversing";
	assert(mState == csDeviceAssigned);
	writeHex4(QString::fromUtf8("reverse:forward:tcp:%1;tcp:%2").arg(aDevicePort).arg(aLocalPort).toUtf8());
	mState = csPortReversing;
}





void AdbCommunicator::shellExecuteV1(const QByteArray & aCommand)
{
	qDebug() << "Sending V1 shell command " << aCommand;
	assert(mState == csDeviceAssigned);
	writeHex4("shell:" + aCommand);
	mState = csExecutingShellV1;
}





QByteArray AdbCommunicator::getAdbPubKey()
{
	auto pubKeyFile = loadAdbPubKeyFile();
	if (pubKeyFile.isEmpty())
	{
		return {};
	}
	auto keyEnd = pubKeyFile.indexOf(' ');
	if (keyEnd < 0)
	{
		return {};
	}
	return QByteArray::fromBase64(pubKeyFile.left(keyEnd));
}





void AdbCommunicator::writeHex4(const QByteArray & aMessage)
{
	auto len = aMessage.length();
	assert(len <= std::numeric_limits<uint16_t>::max());
	auto hex4 = numberToHex4(static_cast<uint16_t>(len));
	mSocket.write(hex4);
	mSocket.write(aMessage);
}





void AdbCommunicator::parseDeviceList(const QByteArray & aMessage)
{
	auto lines = aMessage.split('\n');
	QList<QByteArray> onlineIDs, unauthIDs, otherIDs;
	for (const auto & line: lines)
	{
		if (line.isEmpty())
		{
			continue;
		}
		auto parts = line.split('\t');
		if (parts.size() < 2)
		{
			qDebug() << "Bad DeviceList line received: " << line;
			continue;
		}
		if (parts[0].size() < 8)
		{
			// Suspiciously short ID
			qDebug() << "Bad DeviceList ID received in line " << line;
			continue;
		}
		const auto & status = parts[1];
		if (status == "device")
		{
			onlineIDs.push_back(parts[0]);
		}
		else if ((status == "authorizing") || (status == "unauthorized"))
		{
			unauthIDs.push_back(parts[0]);
		}
		else
		{
			otherIDs.push_back(parts[0]);
		}
	}
	Q_EMIT updateDeviceList(onlineIDs, unauthIDs, otherIDs);
}





bool AdbCommunicator::extractOkayOrFail()
{
	if (mIncomingData.size() < 4)
	{
		// Too small to be a full packet
		return false;
	}
	if (mIncomingData.startsWith("FAIL"))
	{
		if (mIncomingData.size() < 8)
		{
			// Need more data
			return false;
		}
		auto length = hex4ToNumber(mIncomingData.mid(4, 4));
		if (mIncomingData.size() < length + 8)
		{
			// Need more data
			return false;
		}
		auto err = mIncomingData.mid(8, length);
		mIncomingData = mIncomingData.mid(length + 8);
		Q_EMIT error(QString::fromUtf8(err));
		return false;
	}
	else if (mIncomingData.startsWith("OKAY"))
	{
		mIncomingData = mIncomingData.mid(4);
		return true;
	}
	else
	{
		mState = csBroken;
		mSocket.abort();
		Q_EMIT error(tr("Malformed response received from ADB server"));
		return false;
	}
}





std::pair<bool, QByteArray> AdbCommunicator::extractHex4Packet()
{
	if (mIncomingData.size() < 4)
	{
		// Too small to be a full packet
		return {false, {}};
	}

	auto length = hex4ToNumber(mIncomingData.left(4));
	if (mIncomingData.size() < length + 4)
	{
		// Need more data
		return {false, {}};
	}
	auto pkt = mIncomingData.mid(4, length);
	mIncomingData = mIncomingData.mid(4 + length);
	return {true, pkt};
}





void AdbCommunicator::processScreenshot()
{
	assert(static_cast<uint32_t>(mIncomingData.size()) >= mFramebufferSize);
	QImage img(
		reinterpret_cast<uchar *>(mIncomingData.data()),
		static_cast<int>(mFramebufferWidth),
		static_cast<int>(mFramebufferHeight),
		QImage::Format_RGB16
	);
	Q_EMIT screenshotReceived(mAssignedDeviceID, img);
	mIncomingData = mIncomingData.mid(static_cast<int>(mFramebufferSize));
}





void AdbCommunicator::onSocketConnected()
{
	assert(mState == csConnecting);
	mState = csReady;
	Q_EMIT connected();
}





void AdbCommunicator::onSocketError(QAbstractSocket::SocketError aError)
{
	if (aError == QAbstractSocket::RemoteHostClosedError)
	{
		// This is an expected state, don't log or emit anything
		return;
	}

	qDebug() << "Socket error: " << mSocket.errorString();
	mState = csBroken;
	mSocket.abort();
	Q_EMIT error(tr("Error on the underlying TCP socket: %1").arg(mSocket.errorString()));
}





void AdbCommunicator::onSocketDisconnected()
{
	qDebug() << "Socket disconnected: " << mSocket.errorString();
	mState = csBroken;
	mSocket.abort();
	Q_EMIT disconnected();
}





void AdbCommunicator::onSocketReadyRead()
{
	// Append the incoming data:
	while (true)
	{
		auto dataRead = mSocket.read(3000);
		if (dataRead.isEmpty())
		{
			break;
		}
		qDebug() << "Received data: " << dataRead.size() << " bytes: " << dataRead;
		mIncomingData.append(dataRead);
	}

	// Try parsing packets:
	int prevBytesLeft = 0;
	while (mIncomingData.size() != prevBytesLeft)
	{
		prevBytesLeft = mIncomingData.size();
		switch (mState)
		{
			case csCreated:
			case csConnecting:
			case csReady:
			{
				// Invalid state, break the connection
				mState = csBroken;
				Q_EMIT error(tr("Unexpected data received on the socket"));
				break;
			}

			case csBroken:
			{
				// Ignore the data, close socket not to handle any more
				mSocket.close();
				break;
			}

			case csListingDevicesStart:
			{
				if (extractOkayOrFail())
				{
					mState = csListingDevices;
				}
				break;
			}

			case csListingDevices:
			{
				auto packet = extractHex4Packet();
				if (packet.first)
				{
					parseDeviceList(packet.second);
				}
				break;
			}

			case csTrackingDevicesStart:
			{
				if (extractOkayOrFail())
				{
					mState = csTrackingDevices;
				}
				break;
			}

			case csTrackingDevices:
			{
				auto packet = extractHex4Packet();
				if (packet.first)
				{
					parseDeviceList(packet.second);
				}
				break;
			}

			case csAssignDevice:
			{
				if (extractOkayOrFail())
				{
					mState = csDeviceAssigned;
					Q_EMIT deviceAssigned(mAssignedDeviceID);
				}
				break;
			}

			case csDeviceAssigned:
			{
				qDebug() << "Unexpected packet received";
				break;
			}

			case csScreenshottingStart:
			{
				// Check if failure is reported:
				if (!extractOkayOrFail())
				{
					break;
				}

				// Look for the 32-byte header:
				if (mIncomingData.size() < 32)
				{
					break;
				}
				mFramebufferDepth  = decodeLEUInt32(mIncomingData.constData());
				mFramebufferSize   = decodeLEUInt32(mIncomingData.constData() + 4);
				mFramebufferWidth  = decodeLEUInt32(mIncomingData.constData() + 8);
				mFramebufferHeight = decodeLEUInt32(mIncomingData.constData() + 12);
				mIncomingData = mIncomingData.mid(32);
				break;
			}

			case csScreenshotting:
			{
				if (static_cast<uint32_t>(mIncomingData.size()) < mFramebufferSize)
				{
					break;
				}
				processScreenshot();
				break;
			}

			case csPortReversing:
			{
				if (extractOkayOrFail())
				{
					mState = csBroken;  // The connection will be terminated from the device side
					Q_EMIT portReversingEstablished(mAssignedDeviceID);
				}
				break;
			}

			case csExecutingShellV1:
			{
				Q_EMIT shellIncomingData(mAssignedDeviceID, mIncomingData);
				mIncomingData.clear();
				break;
			}
		}
	}
}
