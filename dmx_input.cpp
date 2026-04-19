#include "dmx_input.h"
#include "config.h"
#include "logger.h"
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Réception DMX512 via ESP-IDF UART driver + event queue.
// Détection hardware du BREAK → parsing 100% fiable, pas de désync.
// Compatible Arduino core ESP32 3.x (IDF 5.x) sans dépendance externe.

#define DMX_UART        UART_NUM_1
#define DMX_RX_BUF_SIZE 1024
#define DMX_EVT_QUEUE_LEN 20

static uint8_t       _buffer[513];
static int           _bufferPos = 0;
static uint8_t       _latestData[513];
static unsigned long _lastPacketMs    = 0;
static uint32_t      _packetCount     = 0;
static bool          _everReceived    = false;
static QueueHandle_t _uartQueue       = nullptr;
static bool          _driverInstalled = false;

void dmxInputBegin() {
  memset(_latestData, 0, sizeof(_latestData));
  dmxInputSetPins(DMX_RX_PIN, -1);
}

void dmxInputSetPins(int rxPin, int txPin) {
  if (_driverInstalled) {
    uart_driver_delete(DMX_UART);
    _driverInstalled = false;
    _uartQueue = nullptr;
  }
  uart_config_t cfg = {};
  cfg.baud_rate  = 250000;
  cfg.data_bits  = UART_DATA_8_BITS;
  cfg.parity     = UART_PARITY_DISABLE;
  cfg.stop_bits  = UART_STOP_BITS_2;
  cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
  cfg.source_clk = UART_SCLK_DEFAULT;

  if (uart_driver_install(DMX_UART, DMX_RX_BUF_SIZE, 0, DMX_EVT_QUEUE_LEN, &_uartQueue, 0) != ESP_OK) {
    logMsg("[DMX] uart_driver_install FAILED");
    return;
  }
  uart_param_config(DMX_UART, &cfg);
  uart_set_pin(DMX_UART, txPin, rxPin, -1, -1);
  _driverInstalled = true;
  _bufferPos = 0;
  logMsgf("[DMX] RX on GPIO%d via IDF UART + BREAK detect", rxPin);
}

void dmxInputLoop() {
  if (!_driverInstalled || !_uartQueue) return;
  uart_event_t evt;
  while (xQueueReceive(_uartQueue, &evt, 0) == pdTRUE) {
    switch (evt.type) {
      case UART_DATA: {
        uint8_t tmp[128];
        size_t toRead = evt.size > sizeof(tmp) ? sizeof(tmp) : evt.size;
        int r = uart_read_bytes(DMX_UART, tmp, toRead, 0);
        for (int i = 0; i < r; i++) {
          if (_bufferPos < (int)sizeof(_buffer)) {
            _buffer[_bufferPos++] = tmp[i];
          }
        }
        break;
      }
      case UART_BREAK: {
        // Fin du paquet courant → valider et reset.
        // L'IDF UART driver livre : [byte_frame_err (0x00), start_code (0x00),
        // canal 1, canal 2, ...]. On saute les 2 premiers bytes pour que
        // _latestData[1] = canal 1 DMX.
        if (_bufferPos >= 3) {
          _latestData[0] = 0x00;
          int copyLen = _bufferPos - 2;
          if (copyLen > (int)sizeof(_latestData) - 1) copyLen = (int)sizeof(_latestData) - 1;
          memcpy(_latestData + 1, _buffer + 2, copyLen);
          _lastPacketMs = millis();
          _packetCount++;
          if (!_everReceived) {
            _everReceived = true;
            logMsgf("[DMX] First valid packet (%d bytes)", _bufferPos);
          }
        }
        _bufferPos = 0;
        break;
      }
      case UART_FIFO_OVF:
      case UART_BUFFER_FULL:
        uart_flush_input(DMX_UART);
        _bufferPos = 0;
        break;
      default:
        break;
    }
  }
}

bool dmxInputHasRecentData() {
  if (_lastPacketMs == 0) return false;
  return (millis() - _lastPacketMs) < DMX_SOURCE_TIMEOUT_MS;
}

unsigned long dmxInputLastPacketMs() { return _lastPacketMs; }
uint32_t      dmxInputPacketCount()  { return _packetCount; }

uint8_t dmxInputGet(uint16_t channel) {
  if (channel < 1 || channel > 512) return 0;
  return _latestData[channel];
}
