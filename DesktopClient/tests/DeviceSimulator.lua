#!/bin/env lua
-- Emulates a device, listening for UDP beacons and connecting to the desktop client





--- The following value, when set to true, makes the simulator respond only to Discovery beacon broadcasts
local shouldAnswerToDiscoveryOnly = false





local socket = require("socket")
local tls = require("ssl")

--- Will contain the avatar read from file "Simulator.png", if it exists
local avatar

--- The X509 certificate used for the connection (loaded from SimulatorCert.pem):
local gCert

-- The private key for gCert, loaded from SimulatorPrivKey.pem:
local gPrivKey

-- The public key from gCert:
local gPubKey





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
	assert(type(aNumber) == "number")
	assert(aNumber >= 0)
	assert(aNumber <= 65535)

	local highByte = math.floor(aNumber / 256)
	return string.char(highByte, aNumber - 256 * highByte)
end





--- Converts the specified number to its big-endian uint32 representation and returns it as a four-byte string
local function writeBE32(aNumber)
	assert(type(aNumber) == "number")
	assert(aNumber >= 0)
	assert(aNumber < 65536 * 65536)

	local byte1 = math.floor(aNumber / (65536 * 256))
	aNumber = aNumber - byte1 * 65536 * 256
	local byte2 = math.floor(aNumber / 65536)
	aNumber = aNumber - byte2 * 65536
	local byte3 = math.floor(aNumber / 256)
	aNumber = aNumber - byte3 * 256
	local byte4 = math.floor(aNumber)
	return string.char(byte1, byte2, byte3, byte4)
end





--- Converts the specified number to its big-endian signed int32 representation and returns it as a four-byte string
local function writeBE32s(aNumber)
	assert(type(aNumber) == "number")
	assert(aNumber >= -32768 * 65536)
	assert(aNumber < 32768 * 65536)

	if (aNumber < 0) then
		return writeBE32(65536 * 65536 + aNumber)
	else
		return writeBE32(aNumber)
	end
end





--- Converts the specified number to its big-endian fixed-point-32.32 representation and returns it as an eight-byte string
local function writeBEFP64(aNumber)
	assert(type(aNumber) == "number")
	assert(aNumber >= -32768 * 65536)
	assert(aNumber < 32768 * 65536)

	if (aNumber < 0) then
		aNumber = 65526 * 65526 + aNumber
	end
	local intPart = math.floor(aNumber)
	local fracPart = 65536 * (aNumber - math.floor(aNumber))
	return writeBE32(intPart) .. writeBE32(fracPart)
end





--- Reads a string prefixed by its 2-byte BE length, from aData starting at the specified index
-- Returns the string read and the index of byte following the last byte of the string
local function readBE16LString(aData, aStartIdx)
	assert(type(aData) == "string")

	aStartIdx = aStartIdx or 1
	local len = readBE16(aData, aStartIdx)
	return string.sub(aData, aStartIdx + 2, aStartIdx + 1 + len), aStartIdx + 2 + len
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

	--- Function that is called when the channel is opened, with the init data sent from the remote as its parameter
	-- Returns true on success, nil and error message on failure.
	init = nil,
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
	assert(type(self.mID) == "number")
	assert(self.mID >= 0)
	assert(self.mID <= 65536)

	self.mDevice:sendMuxMessage(self.mID, aMsg)
end






-------------------------------------------------------------------------------------------------------------
-- ChannelInfo class:
local ChannelInfo = Channel:new()





function ChannelInfo:init()
	-- No explicit initialization needed
	return true
end






function ChannelInfo:handleMessage(aMsg)
	assert(type(self) == "table")

	for i = 1, #aMsg, 4 do
		local keyMask = string.sub(aMsg, i, i + 3)
		self:sendValues(keyMask)
	end
end





--- A map of "key" -> value-or-fn for all known ChannelInfo's values
-- A value is serialized directly, a function is called and its first return value is serialized
-- The keys are converted into four-element arrays, "batl" -> {"b", "a", "t", "l"}, for easier comparison
local gInfoValues =
{
	["time"] = os.time,
	["batl"] = function()
		local sec = os.time()
		return math.floor(sec - 100 * math.floor(sec / 100))
	end,
	["sist"] = function()
		return 50 + 50 * math.sin(os.time() / 20)
	end,
	["big "] = function()
		return string.rep("b", 65535)
	end,
	["carr"] = "DeviceSimulatorCarrier",
	["imei"] = "DeviceSimulatorIMEI",
	["imsi"] = "DeviceSimulatorIMSI",
}
-- Process the table's keys into the tabular version:
do
	local tmp = {}
	for k, v in pairs(gInfoValues) do
		local tbl =
		{
			k:sub(1, 1),
			k:sub(2, 2),
			k:sub(3, 3),
			k:sub(4, 4),
		}
		tmp[tbl] = v
	end
	gInfoValues = tmp
	tmp = nil
end




--- Serializes a single value to the wire-transmit format
-- aValue is the value entry in the gInfoValues table, can be a string, number or function
function ChannelInfo:serializeValue(aKey, aValue)
	assert(type(aKey) == "table")
	assert(type(aKey[1]) == "string")
	assert(type(aKey[2]) == "string")
	assert(type(aKey[3]) == "string")
	assert(type(aKey[4]) == "string")
	assert(aValue)

	local key = aKey[1] .. aKey[2] .. aKey[3] .. aKey[4]
	local v = aValue
	if (type(v) == "function") then
		v = v()
		assert(type(v) ~= "function")
	end

	if (type(v) == "number") then
		if (math.floor(v) == v) then
			-- Send as Int32
			print("Sending InfoValue " .. key .. " as Int32: " .. v)
			return key .. string.char(3) .. writeBE32s(v)
		else
			-- Send as FP64
			print("Sending InfoValue " .. key .. " as FP64: " .. v)
			return key .. string.char(6) .. writeBEFP64(v)
		end
	elseif (type(v) == "string") then
		print("Sending InfoValue " .. key .. " as String: " .. v)
		return key .. string.char(7) .. writeBE16(#v) .. v
	else
		error("Invalid value type: " .. type(v) .. " (key " .. aKey .. ", aValue " .. tostring(aValue))
	end
end





--- Sends all our values that match the specified mask
function ChannelInfo:sendValues(aKeyMask)
	assert(type(self) == "table")
	assert(type(aKeyMask) == "string")

	local current = ""
	local lenCurrent = 0
	local keyMask =
	{
		aKeyMask:sub(1, 1),
		aKeyMask:sub(2, 2),
		aKeyMask:sub(3, 3),
		aKeyMask:sub(4, 4),
	}
	for k, v in pairs(gInfoValues) do
		if (
			((keyMask[1] == "?") or (keyMask[1] == k[1])) and
			((keyMask[2] == "?") or (keyMask[2] == k[2])) and
			((keyMask[3] == "?") or (keyMask[3] == k[3])) and
			((keyMask[4] == "?") or (keyMask[4] == k[4]))
		) then
			local serialized = self:serializeValue(k, v)
			local lenSerialized = #serialized
			if (lenSerialized + lenCurrent > 65536) then
				self:sendMessage(current)
				current = serialized
				lenCurrent = lenSerialized
			else
				current = current .. serialized
				lenCurrent = lenCurrent + lenSerialized
			end
		end
	end
	if (lenCurrent > 0) then
		self:sendMessage(current)
	end
end





-------------------------------------------------------------------------------------------------------------
--- A map of "serviceName" -> ChannelClass for all known services / channels:
local gChannelClasses =
{
	["info"] = ChannelInfo,
}





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
		print("Received a Ch0 ping: ReqID " .. aRequestID .. " / Additional data " .. aAdditionalData)
		if (numSuccessfulPings > 10) then
			print("Sending an Error response")
			numSuccessfulPings = 0
			self:sendError(aRequestID, 1000, "Random error")
		else
			numSuccessfulPings = numSuccessfulPings + 1
			self:sendResponse(aRequestID, aAdditionalData)
		end
	elseif (aRequestType == "open") then
		local svcInitData, svcName, nextIdx
		svcName, nextIdx = readBE16LString(aAdditionalData)
		svcInitData, nextIdx = readBE16LString(aAdditionalData, nextIdx)
		local cls = gChannelClasses[svcName]
		if not(cls) then
			print("An unknown service was requested: \"" .. svcName .. "\". Failing the request.")
			self:sendError(aRequestID, ERR_NO_SUCH_SERVICE, svcName)
			return
		end
		local channelID = self.mDevice:findFreeChannelID()
		if not(channelID) then
			print("There's no free ChannelID for the requested service \"" .. svcName .. "\". Failing the request.")
			self:sendError(aRequestID, ERR_NO_CHANNEL_ID, svcName)
			return
		end
		local ch = cls:new({mDevice = self.mDevice, mID = channelID})
		local isOK, msg = ch:init(svcInitData)
		if not(isOK) then
			print("Service \"" .. svcName .. "\" failed to initialize: " .. tostring(msg))
			self:sendError(aRequestID, ERR_SERVICE_INIT_FAILED, "Service failed to initialize: " .. tostring(msg))
			return
		end
		print("Opened a new channel for service \"" .. svcName .. "\", ID " .. channelID)
		self.mDevice.mChannels[channelID] = ch
		self:sendResponse(aRequestID, writeBE16(channelID))
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
		print("Sending our public key")
		self:sendCleartextMessage("pubk", gPubKey)
	elseif (aMsgType == "stls") then
		print("Received a StartTLS request")
		if not(self.mRemotePublicKey) then
			error("The remote is starting TLS without a public key")
		end
		print("Starting TLS, too")
		self:sendCleartextMessage("stls")
		self:switchToMuxProtocol()
	end
end





function Device:switchToMuxProtocol()
	assert(type(self) == "table")
	assert(type(self.mChannels) == "table")

	local params =
	{
		mode = "client",
		protocol = "any",
		key = "SimulatorPrivKey.pem",
		certificate = "SimulatorCert.pem"
	}
	self.mSocket = tls.wrap(self.mSocket, params)
	self.mSocket:dohandshake()
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





--- Finds a ChannelID that is free for a new channel to be opened on
-- Returns the ChannelID, or nil if there's no free ID available
function Device:findFreeChannelID()
	assert(type(self) == "table")
	assert(type(self.mChannels) == "table")

	for i = 0, 65535 do
		if not(self.mChannels[i]) then
			return i
		end
	end
end








--- Returns the contents of the specified file
-- if aIsCompulsory is true, raises an error if the file cannot be read, otherwise returns nil
local function readFile(aFileName, aIsCompulsory)
	local f, err = io.open(aFileName, "rb")
	if not(f) then
		if (aIsCompulsory) then
			error("Cannot read file " .. aFileName .. ": " .. tostring(err))
		else
			return nil
		end
	end
	return f:read("*all")
end





-------------------------------------------------------------------------------------------------------------
-- main:

-- If the Simulator.png file exists, load its contents:
avatar = readFile("Simulator.png", false)

-- Check that the cert and keys exist:
gCert = readFile("SimulatorCert.pem", true)
gPrivKey = readFile("SimulatorPrivKey.pem", true)
gCert = assert(tls.loadcertificate(gCert), "Failed to load certificate")
gPubKey = readFile("SimulatorPubKey.der", true)


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
