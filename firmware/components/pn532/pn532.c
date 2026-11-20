#include "pn532.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "board.h"
#include "pn532_frame.h"

#define PN532_ADDR            0x24    //7 bit i2c address
#define PN532_I2C_HZ          100000
#define I2C_XFER_TIMEOUT_MS   100
#define ACK_TIMEOUT_MS        100
#define READY_POLL_MS         5
#define STATUS_READY          0x01

#define CMD_GET_FIRMWARE      0x02
#define CMD_SAM_CONFIG        0x14
#define CMD_RF_CONFIG         0x32
#define CMD_IN_LIST_PASSIVE   0x4A

static const char *TAG = "pn532";

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;

//Over i2c every read starts with a status byte. bit 0 set = the chip has a frame for me
static bool wait_ready(int timeout_ms) {
  for (int waited = 0; waited <= timeout_ms; waited += READY_POLL_MS) {
    uint8_t status = 0;
    if (i2c_master_receive(s_dev, &status, 1, I2C_XFER_TIMEOUT_MS) == ESP_OK &&
        (status & STATUS_READY)) {
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(READY_POLL_MS));
  }
  return false;
}

//Sends one command and hands back the response data (the bytes after the command byte)
//On the wire it goes: my frame -> PN532 ACK frame -> PN532 response frame
static esp_err_t pn532_command(const uint8_t *cmd, uint8_t n,
                               uint8_t *resp, size_t resp_cap, uint8_t *resp_len,
                               int timeout_ms) {
  static const uint8_t ACK[6] = { 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00 };
  uint8_t tx[PN532_MAX_DATA + PN532_FRAME_OVERHEAD];
  uint8_t rx[1 + PN532_MAX_DATA + PN532_FRAME_OVERHEAD];   //status byte + frame

  size_t tx_len = pn532_build_frame(tx, cmd, n);
  if (tx_len == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  esp_err_t err = i2c_master_transmit(s_dev, tx, tx_len, I2C_XFER_TIMEOUT_MS);
  if (err != ESP_OK) {
    return err;
  }

  //1. the ACK frame, the chip saying "got your command"
  if (!wait_ready(ACK_TIMEOUT_MS)) {
    return ESP_ERR_TIMEOUT;
  }
  err = i2c_master_receive(s_dev, rx, 1 + sizeof ACK, I2C_XFER_TIMEOUT_MS);
  if (err != ESP_OK) {
    return err;
  }
  if (memcmp(rx + 1, ACK, sizeof ACK) != 0) {
    return ESP_ERR_INVALID_RESPONSE;
  }

  //2. the real response, once the chip is done doing the work
  if (!wait_ready(timeout_ms)) {
    return ESP_ERR_TIMEOUT;
  }
  err = i2c_master_receive(s_dev, rx, sizeof rx, I2C_XFER_TIMEOUT_MS);
  if (err != ESP_OK) {
    return err;
  }

  //rx[0] is the status byte, the frame itself starts at rx + 1
  //response command byte is always my command + 1
  pn532_parse_t pr = pn532_parse_response(rx + 1, sizeof rx - 1, (uint8_t)(cmd[0] + 1),
                                          resp, resp_cap, resp_len);
  if (pr != PN532_OK) {
    ESP_LOGW(TAG, "bad response to 0x%02X: %s", cmd[0], pn532_parse_str(pr));
    return ESP_ERR_INVALID_RESPONSE;
  }
  return ESP_OK;
}

esp_err_t pn532_firmware_version(uint8_t *ic, uint8_t *ver, uint8_t *rev) {
  const uint8_t cmd[] = { CMD_GET_FIRMWARE };
  uint8_t resp[8];
  uint8_t n = 0;
  esp_err_t err = pn532_command(cmd, sizeof cmd, resp, sizeof resp, &n, 100);
  if (err != ESP_OK) {
    return err;
  }
  if (n < 4) {
    return ESP_ERR_INVALID_RESPONSE;
  }
  *ic  = resp[0];   //0x32 for a PN532
  *ver = resp[1];
  *rev = resp[2];
  return ESP_OK;
}

esp_err_t pn532_read_uid(uint8_t *uid, uint8_t *uid_len, bool *found) {
  //MaxTg = 1 target, BrTy = 0x00 (106 kbps type A, so MIFARE and NTAG)
  const uint8_t cmd[] = { CMD_IN_LIST_PASSIVE, 0x01, 0x00 };
  uint8_t resp[PN532_MAX_DATA];
  uint8_t n = 0;

  *found = false;
  esp_err_t err = pn532_command(cmd, sizeof cmd, resp, sizeof resp, &n, 300);
  if (err != ESP_OK) {
    return err;
  }
  //resp layout: NbTg | Tg | SENS_RES(2) | SEL_RES | NFCIDLength | NFCID...
  if (n < 1 || resp[0] == 0) {
    return ESP_OK;   //no tag in the field, thats not an error
  }
  if (n < 6) {
    return ESP_ERR_INVALID_RESPONSE;
  }
  uint8_t id_len = resp[5];
  if (id_len == 0 || id_len > 7 || (size_t)6 + id_len > n) {
    return ESP_ERR_INVALID_RESPONSE;   //never trust a length field blindly
  }
  memcpy(uid, &resp[6], id_len);
  *uid_len = id_len;
  *found = true;
  return ESP_OK;
}

esp_err_t pn532_init(void) {
  const i2c_master_bus_config_t bus_cfg = {
    .i2c_port          = I2C_NUM_0,
    .sda_io_num        = PIN_I2C_SDA,
    .scl_io_num        = PIN_I2C_SCL,
    .clk_source        = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,   //the module has its own pullups too
  };
  esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
  if (err != ESP_OK) {
    return err;
  }

  const i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address  = PN532_ADDR,
    .scl_speed_hz    = PN532_I2C_HZ,
    .scl_wait_us     = 20000,   //the PN532 stretches the clock, give it time
  };
  err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
  if (err != ESP_OK) {
    return err;
  }

  //step 1: is anything even answering at 0x24?
  err = i2c_master_probe(s_bus, PN532_ADDR, I2C_XFER_TIMEOUT_MS);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "no device at 0x%02X: check wiring and DIP switches", PN532_ADDR);
    return err;
  }

  //step 2: firmware version. first command after power up can time out
  //while the chip is still waking up, so give it a few tries
  uint8_t ic = 0, ver = 0, rev = 0;
  for (int attempt = 0; attempt < 3; attempt++) {
    err = pn532_firmware_version(&ic, &ver, &rev);
    if (err == ESP_OK) {
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "GetFirmwareVersion failed: %s", esp_err_to_name(err));
    return err;
  }
  ESP_LOGI(TAG, "found PN5%02X, firmware %u.%u", ic, ver, rev);

  uint8_t resp[4];
  uint8_t n = 0;

  //step 3: SAMConfiguration, normal mode, no timeout, no IRQ pin
  const uint8_t sam[] = { CMD_SAM_CONFIG, 0x01, 0x14, 0x00 };
  err = pn532_command(sam, sizeof sam, resp, sizeof resp, &n, 100);
  if (err != ESP_OK) {
    return err;
  }

  //step 4: RFConfiguration item 5 (MaxRetries). last byte caps how long
  //InListPassiveTarget hunts for a tag, default is forever
  const uint8_t rf[] = { CMD_RF_CONFIG, 0x05, 0xFF, 0x01, 0x02 };
  return pn532_command(rf, sizeof rf, resp, sizeof resp, &n, 100);
}
