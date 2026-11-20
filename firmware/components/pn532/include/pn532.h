#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

//PN532 NFC reader talking over I2C

esp_err_t pn532_init(void);   //sets up the i2c bus and wakes the chip
esp_err_t pn532_firmware_version(uint8_t *ic, uint8_t *ver, uint8_t *rev);

//Looks for one ISO14443A tag. *found = false just means nothing in the field, not an error
//uid needs room for 7 bytes
esp_err_t pn532_read_uid(uint8_t *uid, uint8_t *uid_len, bool *found);

//Task: keeps polling for tags and posts EVT_TAG when a new one shows up
void nfc_task_start(void);
bool nfc_is_ready(void);   //true once the reader has talked back to me
