#include "Device.hpp"
#include <cassert>





Device::Device(ConnectionPtr aConnection):
	mDeviceID(aConnection->remotePublicID().value()),
	mConnections({aConnection})
{
	assert(!mDeviceID.isEmpty());
}





void Device::addConnection(ConnectionPtr aConnection)
{
	mConnections.push_back(aConnection);
	emit connectionAdded(this->shared_from_this(), aConnection);
}





const QString & Device::friendlyName() const
{
	if (mConnections.empty())
	{
		static const QString empty;
		return empty;
	}
	return mConnections[0]->friendlyName().value();
}
