#include "artnet_input.h"
#include "config.h"
#include "logger.h"
#include <AsyncUDP.h>

// Protocole Art-Net OpDmx (OpOutput) :
//   0..7  : "Art-Net\0"
//   8..9  : OpCode (little-endian) = 0x5000
//   10..11: Protocol version (big-endian) = 14
//   12    : Sequence
//   13    : Physical
//   14    : SubUni (sub-net high nibble, universe low nibble)
//   15    : Net
//   16..17: Length (big-endian)
//   18..  : DMX data (up to 512 bytes, NO start code)

static AsyncUDP      _udp;
static uint8_t       _latestData[513];  // 0 = start code (toujours 0), 1..512 = canaux
static unsigned long _lastPacketMs = 0;
static uint32_t      _packetCount  = 0;
static uint8_t       _filterNet     = 0;
static uint8_t       _filterSubnet  = 0;
static uint8_t       _filterUni     = 0;
static bool          _everReceived  = false;
static uint16_t      _lastDataLen   = 0;
static uint16_t      _lastRawLen    = 0;
static uint8_t       _lastSeenNet   = 0;
static uint8_t       _lastSeenSub   = 0;
static uint8_t       _lastSeenUni   = 0;
static uint32_t      _totalSeen     = 0;

static void handlePacket(AsyncUDPPacket& pkt) {
  const uint8_t* data = pkt.data();
  size_t len = pkt.length();
  if (len < 18) return;
  static const uint8_t kArtNetID[8] = {'A','r','t','-','N','e','t', 0};
  for (int i = 0; i < 8; i++) if (data[i] != kArtNetID[i]) return;
  uint16_t opcode = (uint16_t)data[8] | ((uint16_t)data[9] << 8);  // little-endian
  if (opcode != 0x5000) return;

  uint8_t subUni = data[14];
  uint8_t net    = data[15];
  uint8_t uni    = subUni & 0x0F;
  uint8_t sub    = (subUni >> 4) & 0x0F;
  _lastSeenNet = net;
  _lastSeenSub = sub;
  _lastSeenUni = uni;
  _totalSeen++;
  if (net != _filterNet || sub != _filterSubnet || uni != _filterUni) return;

  uint16_t dataLen = ((uint16_t)data[16] << 8) | (uint16_t)data[17];
  if (dataLen > 512) dataLen = 512;
  if (18 + dataLen > len) dataLen = (len > 18) ? (uint16_t)(len - 18) : 0;

  _latestData[0] = 0x00;  // start code implicite en Art-Net
  memcpy(_latestData + 1, data + 18, dataLen);
  _lastPacketMs = millis();
  _packetCount++;
  _lastDataLen  = dataLen;
  _lastRawLen   = (uint16_t)len;
  if (!_everReceived) {
    _everReceived = true;
    logMsgf("[ArtNet] First packet received (net=%d sub=%d uni=%d len=%d)", net, sub, uni, dataLen);
  }
}

void artnetInputBegin(uint8_t net, uint8_t subnet, uint8_t universe) {
  _filterNet    = net;
  _filterSubnet = subnet;
  _filterUni    = universe;
  if (_udp.listen(ARTNET_PORT)) {
    _udp.onPacket(handlePacket);
    logMsgf("[ArtNet] Listening UDP %d net=%d sub=%d uni=%d", ARTNET_PORT, net, subnet, universe);
  } else {
    logMsgf("[ArtNet] Failed to bind UDP port %d", ARTNET_PORT);
  }
}

void artnetInputSetFilter(uint8_t net, uint8_t subnet, uint8_t universe) {
  _filterNet    = net;
  _filterSubnet = subnet;
  _filterUni    = universe;
  logMsgf("[ArtNet] Filter updated net=%d sub=%d uni=%d", net, subnet, universe);
}

bool artnetInputHasRecentData() {
  if (_lastPacketMs == 0) return false;
  return (millis() - _lastPacketMs) < DMX_SOURCE_TIMEOUT_MS;
}

unsigned long artnetInputLastPacketMs() { return _lastPacketMs; }
uint32_t      artnetInputPacketCount()  { return _packetCount; }

uint8_t artnetInputGet(uint16_t channel) {
  if (channel < 1 || channel > 512) return 0;
  return _latestData[channel];
}

uint16_t artnetInputLastDataLen()      { return _lastDataLen; }
uint16_t artnetInputLastPacketRawLen() { return _lastRawLen; }
uint8_t  artnetInputLastNet()          { return _lastSeenNet; }
uint8_t  artnetInputLastSub()          { return _lastSeenSub; }
uint8_t  artnetInputLastUni()          { return _lastSeenUni; }
uint32_t artnetInputTotalSeen()        { return _totalSeen; }
