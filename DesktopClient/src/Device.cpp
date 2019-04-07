#include "Device.hpp"
#include <cassert>





Device::Device(ConnectionPtr aConnection):
	mDeviceID(aConnection->remotePublicID().value())
{
	assert(!mDeviceID.isEmpty());
}





void Device::addConnection(ConnectionPtr aConnection)
{
	mConnections.push_back(aConnection);
	emit connectionAdded(this->shared_from_this(), aConnection);
}
