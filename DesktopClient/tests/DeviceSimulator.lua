-- Emulates a device, listening for UDP beacons and connecting to the desktop client





--- Reads the two bytes from aData starting at the specified index (1-based) and returns
-- the number represented by them (big-endian uint16)
local function readBE16(aData, aStartIdx)
	assert(type(aData) == "string")
	aStartIdx = aStartIdx or 1

	return string.byte(aData, aStartIdx) * 256 + string.byte(aData, aStartIdx + 1)
end





local socket = require("socket")
local udpReceiver = assert(socket.udp())
assert(udpReceiver:setsockname("*", 24816))
print("Waiting for UDP beacon...")
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
	error("The received PublicID is too short (expected >15, got " .. idLen .. ")")
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

-- TODO: Try connecting to the Deskemes client

print("All done")
