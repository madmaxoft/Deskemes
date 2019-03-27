#pragma once

#include <memory>
#include <QByteArray>





// fwd:
class Connection;
class ComponentCollection;





/** Base class for all protocol phase handlers.
Its job is to understand the incoming data, handle it and potentially switch the connection to another PhaseHandler. */
class PhaseHandler
{
public:

	/** Creates a PhaseHandler tied to the specified connection. */
	PhaseHandler(Connection & aConnection);

	// Force a virtual destructor in descendants
	virtual ~PhaseHandler() {}

	/** Processes the next part of the incoming data.
	Called by Connection when it reads more data from the transport.
	If the descendant replaces the Connection's PhaseHandler, it is responsible for relaying the adequate
	leftover data from aData into the new PhaseHandler.
	The descendants should clear mOldPhaseHandler just before exiting this function. */
	virtual void processIncomingData(const QByteArray & aData) = 0;

	/** Writes the data to the remote peer.
	Overridable so that descendants may transform the data before sending.
	The default implementation simply calls mConnection.write(). */
	virtual void write(const QByteArray & aData);


protected:

	/** The connection for which the phase handler handles the data. */
	Connection & mConnection;

	/** Storage for the old phase handler.
	Used when changing the phase handler in mConnection; the old handler (presumably this object) is put
	into this member to keep it alive until its processing is finished. The descendants need to clear
	this member when they finish processing, typically at the end of processIncomingData(). */
	std::unique_ptr<PhaseHandler> mOldPhaseHandler;
};





/** Base class for protocol phase handlers that implement a message-based protocol.
Implements the incoming data buffer for holding incomplete messages. */
class PhaseHandlerMsgBase:
	public PhaseHandler
{
	using Super = PhaseHandler;

public:
	// No special constructor needed
	using Super::Super;

	// PhaseHandler overrides:
	virtual void processIncomingData(const QByteArray & aData) override;

	/** Extracts a complete protocol message from mIncomingData, and acts on it.
	Returns true if a complete message was extracted, false if there's no complete message.
	Provided by descendants to actually implement the protocol. */
	virtual bool extractAndHandleMessage() = 0;


protected:

	/** Buffer for the unprocessed incoming data.
	The data is stored here until a full message can be extracted; it is then removed from the buffer. */
	QByteArray mIncomingData;


	/** Terminates the connection and clears the incoming data buffer, so that no more messages are parsed. */
	void abortConnection();
};





/** The cleartext request / response protocol handler. */
class PhaseHandlerCleartext:
	public PhaseHandlerMsgBase
{
	using Super = PhaseHandlerMsgBase;

public:

	PhaseHandlerCleartext(ComponentCollection & aComponents, Connection & aConnection);

	// PhaseHandlerMsgBase overrides:
	virtual bool extractAndHandleMessage() override;


protected:

	/** The components of the entire app. */
	ComponentCollection & mComponents;

	/** True after the initial protocol identification has been received.
	Used to detect messages sent without proper identification first. */
	bool mHasReceivedIdentification;

	/** The protocol version the remote has indicated in their protocol ident. */
	quint16 mRemoteVersion;

	/** Set to true after the StartTls request has been sent
	No more messages are allowed to be sent from us. */
	bool mHasSentStartTls;


	/** Handles the specified message. */
	void handleMessage(const quint32 aMsgType, const QByteArray & aMsg);

	/** Handles the "dsms" message. */
	void handleMessageDsms(const QByteArray & aMsg);

	/** Handles the "pubk" message. */
	void handleMessagePubk(const QByteArray & aMsg);

	/** Handles the "stlt" message, starting the TLS, if appropriate. */
	void handleMessageStls();

	/** Sends the specified message over to the remote peer. */
	void sendMessage(quint32 aMsgType, const QByteArray & aMsg = QByteArray());
};





/** The encrypted phase of the protocol, minus the encryption.
The encryption itself is handled in a filter-PhaseHandler that then uses this class to do the actual
protocol parsing.
This object handles the channel muxing. */
class PhaseHandlerMuxer:
	public PhaseHandlerMsgBase
{
	using Super = PhaseHandlerMsgBase;

public:

	PhaseHandlerMuxer(Connection & aConnection);


	// PhaseHandlerMsgBase overrides:
	virtual bool extractAndHandleMessage() override;
};
