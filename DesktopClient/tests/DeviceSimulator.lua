-- Emulates a device, listening for UDP beacons and connecting to the desktop client





--- The following value, when set to true, makes the simulator respond only to Discovery beacon broadcasts
local shouldAnswerToDiscoveryOnly = false





local socket = require("socket")

--- Will contain the avatar read from file "Simulator.png", if it exists
local avatar





-- Common error codes:
local ERR_UNSUPPORTED_REQTYPE = 1
local ERR_NO_SUCH_SERVICE = 2
local ERR_NO_CHANNEL_ID = 3
local ERR_SERVICE_INIT_FAILED = 4
local ERR_NO_PERMISSION = 5
local ERR_NOT_YET = 6
local ERR_NO_REQUEST_ID = 7





--- Reads the two bytes from aData starting at the specified index (1-based) and returns
-- the number represented by them (big-endian uint16)
local function readBE16(aData, aStartIdx)
	assert(type(aData) == "string")
	aStartIdx = aStartIdx or 1

	return string.byte(aData, aStartIdx) * 256 + string.byte(aData, aStartIdx + 1)
end





--- Converts the specified number to its big-endian uint16 representation and returns it as a two-byte string
local function writeBE16(aNumber)
	assert(aNumber >= 0)
	assert(aNumber <= 65535)
	local highByte = math.floor(aNumber / 256)
	return string.char(highByte, aNumber - 256 * highByte)
end





local hexChar = {[0] = "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c", "d", "e", "f"}
--- Converts the input string to a string representing the input characters in hex:
-- "abc" -> "414243"
local function toHex(aStr)
	assert(type(aStr) == "string")

	return string.gsub(aStr, ".",
		function(ch)
			local b = string.byte(ch, 1)
			local highNibble = math.floor(b / 16)
			return hexChar[highNibble] .. hexChar[b - 16 * highNibble]
		end
	)
end





-------------------------------------------------------------------------------------------------------------
-- Channel class:

-- The implementation of a single mux-protocol channel in a device
local Channel =
{
	-- The Device instance for which the channel is created
	mDevice = nil,

	-- The ChannelID within the mux protocol for this channel
	mID = -1,

	-- Function that is called to process messages incoming on the channel
	handleMessage = nil,
}





function Channel:new(aObj)
	aObj = aObj or {}
	setmetatable(aObj, self)
	self.__index = self
	return aObj
end





--- Sends the specified message to the remote on this muxed channel
function Channel:sendMessage(aMsg)
	assert(type(self) == "table")
	assert(type(self.mDevice) == "table")

	self.mDevice:sendMuxMessage(self.mID, aMsg)
end





-------------------------------------------------------------------------------------------------------------
-- ChannelZero class:
local ChannelZero = Channel:new()





--- Handles the message coming in to the command channel
-- Hands off the processing to handleRequest(), handleResponse() or handleError() based on message type
function ChannelZero:handleMessage(aMsg)
	assert(type(self) == "table")
	assert(type(aMsg) == "string")

	local len = #aMsg
	if (len < 4) then
		error("Message too short")
	end

	local msgType = string.byte(aMsg, 1)
	local reqID = string.byte(aMsg, 2)
	if (msgType == 0) then
		-- Request
		if (len < 6) then
			error("Request message too short")
		end
		local reqType = string.sub(aMsg, 3, 6)
		return self:handleRequest(reqType, reqID, string.sub(aMsg, 7))
	elseif (msgType == 1) then
		-- Response
		-- TODO
	elseif (msgType == 2) then
		-- Error
		print("Received an error response:")
		print("\tReqID = " .. string.byte(aMsg, 2))
		print("\tErrorCode = " .. readBE16(aMsg, 3))
		print("\tData = " .. string.sub(aMsg, 5))
	else
		error("Unknown message type")
	end
end





local numSuccessfulPings = 0

--- Handles the request received from the remote
function ChannelZero:handleRequest(aRequestType, aRequestID, aAdditionalData)
	assert(type(self) == "table")
	assert(type(aRequestType) == "string")
	assert(type(aRequestID) == "number")
	assert(type(aAdditionalData or "") == "string")

	if (aRequestType == "ping") then
		print("Received a Ch0 ping " .. aRequestID .. " / " .. aAdditionalData)
		if (numSuccessfulPings > 10) then
			print("Sending an Error response")
			numSuccessfulPings = 0
			self:sendError(aRequestID, 1000, "Random error")
		else
			numSuccessfulPings = numSuccessfulPings + 1
			self:sendResponse(aRequestID, aAdditionalData)
		end
	elseif (aRequestType == "open") then
		-- TODO
	elseif (aRequestType == "clse") then
		-- TODO
	else
		self:sendError(aRequestID, ERR_UNSUPPORTED_REQTYPE, "The request type \"" .. aRequestType .. "\" is not supported")
	end
end





--- Sends a Success response to the remote
function ChannelZero:sendResponse(aRequestID, aAdditionalData)
	assert(type(self) == "table")
	assert(type(aRequestID) == "number")
	assert(type(aAdditionalData or "") == "string")

	self:sendMessage(string.char(1) .. string.char(aRequestID) .. (aAdditionalData or ""))
end





--- Sends an Error response to the remote
function ChannelZero:sendError(aRequestID, aErrorCode, aMessage)
	assert(type(self) == "table")
	assert(type(aRequestID) == "number")
	assert(type(aErrorCode) == "number")
	assert(type(aMessage or "") == "string")

	self:sendMessage(string.char(2) .. string.char(aRequestID) .. writeBE16(aErrorCode) .. aMessage)
end





-------------------------------------------------------------------------------------------------------------
-- Device class:

--- The implementation of the simulated device
local Device =
{
	-- The socket used for communication
	mSocket = nil,

	-- The incoming data, buffered until a complete protocol message is present
	mIncomingData = nil,

	-- The function that handles incoming data.
	-- Starts as Device:extractProcessCleartextMessage(), then is substituted to Device:extractProcessMuxMessage() once TLS is reached
	extractProcessMessage = nil,

	-- Map of ChannelID -> Channel instance for all currently open channels
	mChannels = {},
}





--- Creates a new instance of a Device class
function Device:new(aObj)
	aObj = aObj or {}
	aObj.mChannels = aObj.mChannels or {}
	aObj.mChannels[0] = Channel:new(aObj.mChannels[0])
	setmetatable(aObj, self)
	self.__index = self
	return aObj
end





--- Sends the specified single cleartext protocol message
-- aMsgData may be either nil (for empty payload) or a string
function Device:sendCleartextMessage(aMsgType, aMsgData)
	assert(type(aMsgType) == "string")
	assert(#aMsgType == 4)
	assert((type(aMsgData) == "nil") or (type(aMsgData) == "string"))

	aMsgData = aMsgData or ""
	self.mSocket:send(aMsgType .. writeBE16(#aMsgData) .. aMsgData)
end





--- Processes a single cleartext protocol message
function Device:processCleartextMessage(aMsgType, aMsgData)
	assert(type(self) == "table")

	if (aMsgType ~= "dsms") then
		if not(self.mHasReceivedIdentification) then
			error("Received a message before protocol identification: " .. aMsgType)
		end
	end

	if (aMsgType == "dsms") then
		if (#aMsgData < 10) then
			error("Received a too short protocol identification (exp >=10, got " .. #aMsgData .. " bytes)")
		end
		if (string.sub(aMsgData, 1, 8) ~= "Deskemes") then
			error("Received an invalid protocol identification")
		end
		print("Received a protocol identification, version " .. readBE16(aMsgData, 9))
		self.mHasReceivedIdentification = true
		print("Sending my protocol identification...")
		self:sendCleartextMessage("dsms", "Deskemes\0\1")
		self:sendCleartextMessage("fnam", "DeviceSimulator")
		if (avatar) then
			self:sendCleartextMessage("avtr", avatar)
		end
		self:sendCleartextMessage("pubi", "DeviceSimulatorPublicID")

		--[[
		-- DEBUG: This should terminate the connection, since no pubkey has been transmitted:
		print("Sending TLS request, expecting the connection to terminate.")
		self:sendCleartextMessage("stls")
		--]]


	elseif (aMsgType == "fnam") then
		print("Received friendy name: " .. aMsgData)
	elseif (aMsgType == "avtr") then
		print("Received avatar: " .. #aMsgData .. " bytes")
	elseif (aMsgType == "pubi") then
		print("Received public ID: 0x" .. toHex(aMsgData))
		if (aMsgData ~= self.mBeaconPublicID) then
			error("The public ID received through TCP is different than the UDP-broadcast one.")
		end
		self.mRemotePublicID = aMsgData
	elseif (aMsgType == "pubk") then
		print("Received client's public key")
		if not(self.mRemotePublicID) then
			error("The remote sent its public key without sending its public ID first")
		end
		self.mRemotePublicKey = aMsgData

		-- socket.select(nil, nil, 1)  -- Wait 1 second

		print("Sending our (dummy) public key")
		self:sendCleartextMessage("pubk", "DummySimulatorPublicKeyData")
		--[[
		print("Sending TLS request.")
		self:sendCleartextMessage("stls")
		--]]
	elseif (aMsgType == "stls") then
		print("Received a StartTLS request")
		if not(self.mRemotePublicKey) then
			error("The remote is starting TLS without a public key")
		end

		-- socket.select(nil, nil, 1)  -- Wait 1 second

		print("Starting TLS, too")
		self:sendCleartextMessage("stls")

		-- TODO: TLS
		self:switchToMuxProtocol()
	end
end





function Device:switchToMuxProtocol()
	assert(type(self) == "table")
	assert(type(self.mChannels) == "table")

	self.extractProcessMessage = self.extractProcessMuxMessage
	self.mChannels[0] = ChannelZero:new({mDevice = self, mID = 0})
end





--- Processes a single mux-protocol message sent to the specified channel
function Device:processMuxChannelMessage(aChannelID, aMsg)
	assert(type(self) == "table")
	assert(type(aChannelID) == "number")
	assert(type(aMsg) == "string")

	local channel = self.mChannels[aChannelID]
	if not(channel) then
		error("Received a mux-message for non-existent channel " .. aChannelID)
		return
	end

	channel:handleMessage(aMsg)
end





--- Tries to extract a single mux-protocol message, and send it to processing
-- Returns true if extracted a message, false if not
function Device:extractProcessMuxMessage()
	assert(type(self) == "table")
	assert(type(self.mIncomingData) == "string")

	local len = #(self.mIncomingData)
	if (len < 4) then
		-- Not enough data for even an empty message
		return false
	end
	local dataLen = readBE16(self.mIncomingData, 3)
	if (len < dataLen + 4) then
		-- Not a complete message yet
		return false
	end
	local channelID = readBE16(self.mIncomingData, 1)
	self:processMuxChannelMessage(channelID, string.sub(self.mIncomingData, 5, 4 + dataLen))
	self.mIncomingData = string.sub(self.mIncomingData, 5 + dataLen)
	return true
end





--- Tries to extract a single cleartext-protocol message, and send it to processing
-- Returns true if extracted a message, false if not
function Device:extractProcessCleartextMessage()
	assert(type(self) == "table")
	assert(type(self.mIncomingData) == "string")

	local len = #(self.mIncomingData)
	if (len < 6) then
		-- Not enough data for even an empty message
		return false
	end

	local msgLen = readBE16(self.mIncomingData, 5)
	if (len < 6 + msgLen) then
		-- Not enough data for this particular message
		return false
	end

	local msgType = string.sub(self.mIncomingData, 1, 4)
	local msgData = string.sub(self.mIncomingData, 7, 6 + msgLen)
	self:processCleartextMessage(msgType, msgData)
	self.mIncomingData = string.sub(self.mIncomingData, 7 + msgLen)
	return true
end





--- Connects to the client at the specified TCP endpoint and emulates a device
function Device:connect(aIP, aTcpPort, aBeaconPublicID)
	assert(type(self) == "table")
	assert(type(aIP) == "string")
	assert(tonumber(aTcpPort))

	self.mBeaconPublicID = aBeaconPublicID
	print("Connecting as a device to " .. aIP .. ":" .. aTcpPort .. "...")
	self.mSocket = socket.connect(aIP, aTcpPort)
	if not(self.mSocket) then
		print("Connection failed")
		return
	end
	self.mSocket:settimeout(0.1)
	print("Connected, processing...")
	self.mIncomingData = ""
	self.extractProcessMessage = self.extractProcessCleartextMessage  -- Initial protocol is the Cleartext
	while (true) do

		-- Read the data from the socket:
		local data, err, partial = self.mSocket:receive(30)
		if not(data) then
			if (err == "timeout") then
				data = partial
			elseif (err == "closed") then
				if (partial == "") then
					print("Socket closed and all data processed")
					return
				end
				data = partial
			else
				print("Socket error: " .. err)
				self.mSocket:close()
				return
			end
		end

		-- Store and process the data:
		if (data ~= "") then
			self.mIncomingData = self.mIncomingData .. data
			while (self:extractProcessMessage()) do
				-- Empty loop body on purpose
			end
		end
	end
end





--- Sends the specified message to the remote on this muxed channel
function Device:sendMuxMessage(aChannelID, aMsg)
	assert(type(self) == "table")
	assert(type(aChannelID) == "number")
	assert(type(aMsg or "") == "string")

	local len = #aMsg
	assert(len < 65536)

	self.mSocket:send(writeBE16(aChannelID) .. writeBE16(len) .. aMsg)
end





-------------------------------------------------------------------------------------------------------------
-- main:

-- If the Simulator.png file exists, load its contents:
local f = io.open("Simulator.png", "rb")
if (f) then
	avatar = f:read("*all")
	f:close()
end

-- Main loop: Listen for a single UDP beacon and simulate a device for it:
local udpReceiver = assert(socket.udp())
assert(udpReceiver:setsockname("*", 24816))
while (true) do
	print("**** Waiting for UDP beacon...")
	local packet, fromIP, fromPort = udpReceiver:receivefrom()
	if not(packet) then
		error(fromIP)
	end
	print("Received a UDP packet from " .. fromIP .. ":" .. fromPort .. ", " .. #packet .. " bytes")

	-- Sanity-check the packet:
	if (#packet < 15) then
		error("The received UDP packet is too short to be a valid Deskemes beacon")
	end
	if (string.sub(packet, 1, 8) ~= "Deskemes") then
		error("The received UDP packet is not a valid Deskemes beacon")
	end
	local version = readBE16(packet, 9)
	if (version ~= 1) then
		error("The received UDP packet is incorrect version (expected 1, got " .. version .. ")")
	end
	local idLen = readBE16(packet, 11)
	if (idLen < 16) then
		error("The received PublicID is too short (expected >=16, got " .. idLen .. ")")
	end
	if (#packet < 15 + idLen) then
		error("The received packet is too short to contain the PublicID (expected " .. 16 + idLen .. ", got " .. #packet .. " bytes)")
	end

	-- Extract the data:
	local publicID = string.sub(packet, 13, 12 + idLen)
	local tcpPort = readBE16(packet, 13 + idLen)
	local isDiscovery = (string.byte(packet, 15 + idLen) ~= 0)
	print("TCP port: " .. tostring(tcpPort))
	print("Is discovery: " .. tostring(isDiscovery))

	if (shouldAnswerToDiscoveryOnly and not(isDiscovery)) then
		print("Ignoring a non-discovery beacon")
	else
		local dev = Device:new()
		dev:connect(fromIP, tcpPort, publicID)
		-- The device has done all its work in connect(), so now it's disconnected again
		print("Device not connected")
	end

	-- Flush packets received while blocked in the device loop:
	assert(udpReceiver:settimeout(0.1))  -- abort if blocking for too long
	while (packet) do
		packet = udpReceiver:receivefrom()
	end
	assert(udpReceiver:settimeout())  -- blocking
end
print("All done")
