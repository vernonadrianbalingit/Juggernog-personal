#include "check.h"
#include "../components/pn532/pn532_frame.c" //pull in the code being tested
#include "pn532_frame.h"

int main(void) {
  uint8_t out[PN532_MAX_DATA + PN532_FRAME_OVERHEAD];

  //--- Building: compare against bytes i worked out by hand from the PN532 manual

  const uint8_t get_fw[] = { 0x02 };
  const uint8_t get_fw_wire[] = { 0x00, 0x00, 0xFF, 0x02, 0xFE, 0xD4, 0x02, 0x2A, 0x00 };
  CHECK(pn532_build_frame(out, get_fw, sizeof get_fw) == sizeof get_fw_wire);
  CHECK_BYTES(out, get_fw_wire, sizeof get_fw_wire);

  const uint8_t sam[] = { 0x14, 0x01, 0x14, 0x00 };
  const uint8_t sam_wire[] = { 0x00, 0x00, 0xFF, 0x05, 0xFB,
                               0xD4, 0x14, 0x01, 0x14, 0x00, 0x03, 0x00 };
  CHECK(pn532_build_frame(out, sam, sizeof sam) == sizeof sam_wire);
  CHECK_BYTES(out, sam_wire, sizeof sam_wire);

  const uint8_t rf[] = { 0x32, 0x05, 0xFF, 0x01, 0x02 };
  const uint8_t rf_wire[] = { 0x00, 0x00, 0xFF, 0x06, 0xFA,
                              0xD4, 0x32, 0x05, 0xFF, 0x01, 0x02, 0xF3, 0x00 };
  CHECK(pn532_build_frame(out, rf, sizeof rf) == sizeof rf_wire);
  CHECK_BYTES(out, rf_wire, sizeof rf_wire);

  const uint8_t list[] = { 0x4A, 0x01, 0x00 };
  const uint8_t list_wire[] = { 0x00, 0x00, 0xFF, 0x04, 0xFC,
                                0xD4, 0x4A, 0x01, 0x00, 0xE1, 0x00 };
  CHECK(pn532_build_frame(out, list, sizeof list) == sizeof list_wire);
  CHECK_BYTES(out, list_wire, sizeof list_wire);

  CHECK(pn532_build_frame(out, get_fw, 0) == 0);                    //nothing to send
  CHECK(pn532_build_frame(out, get_fw, PN532_MAX_DATA + 1) == 0);   //too long for the buffer

  //--- Parsing a good frame: GetFirmwareVersion response, IC 0x32, firmware v1.6
  const uint8_t fw_resp[] = { 0x00, 0x00, 0xFF, 0x06, 0xFA,
                              0xD5, 0x03, 0x32, 0x01, 0x06, 0x07, 0xE8, 0x00 };
  uint8_t data[16];
  uint8_t n = 0;
  CHECK(pn532_parse_response(fw_resp, sizeof fw_resp, 0x03, data, sizeof data, &n) == PN532_OK);
  CHECK(n == 4);
  const uint8_t fw_data[] = { 0x32, 0x01, 0x06, 0x07 };
  CHECK_BYTES(data, fw_data, sizeof fw_data);

  //extra junk after the frame is fine, the PN532 pads its i2c reads
  uint8_t padded[24] = { 0 };
  memcpy(padded, fw_resp, sizeof fw_resp);
  CHECK(pn532_parse_response(padded, sizeof padded, 0x03, data, sizeof data, &n) == PN532_OK);

  //--- Parsing bad frames: break one thing per check, each should hit its own error
  uint8_t bad[sizeof fw_resp];

  CHECK(pn532_parse_response(fw_resp, 7, 0x03, data, sizeof data, &n) == PN532_ERR_TOO_SHORT);

  memcpy(bad, fw_resp, sizeof bad);
  bad[2] = 0xFE;
  CHECK(pn532_parse_response(bad, sizeof bad, 0x03, data, sizeof data, &n) == PN532_ERR_PREAMBLE);

  memcpy(bad, fw_resp, sizeof bad);
  bad[4] = 0xFB;
  CHECK(pn532_parse_response(bad, sizeof bad, 0x03, data, sizeof data, &n) == PN532_ERR_LEN_CHECKSUM);

  //LEN claims 0x20 bytes (with a matching LCS so the checksum passes) but i only have 13.
  //the bounds check has to catch this before any data gets read
  memcpy(bad, fw_resp, sizeof bad);
  bad[3] = 0x20;
  bad[4] = 0xE0;
  CHECK(pn532_parse_response(bad, sizeof bad, 0x03, data, sizeof data, &n) == PN532_ERR_LEN_RANGE);

  memcpy(bad, fw_resp, sizeof bad);
  bad[5] = 0xD4;
  CHECK(pn532_parse_response(bad, sizeof bad, 0x03, data, sizeof data, &n) == PN532_ERR_TFI);

  memcpy(bad, fw_resp, sizeof bad);
  bad[8] ^= 0x01;   //flip one bit in the data
  CHECK(pn532_parse_response(bad, sizeof bad, 0x03, data, sizeof data, &n) == PN532_ERR_DATA_CHECKSUM);

  CHECK(pn532_parse_response(fw_resp, sizeof fw_resp, 0x4B, data, sizeof data, &n) == PN532_ERR_COMMAND);

  //callers buffer is too small for the data
  CHECK(pn532_parse_response(fw_resp, sizeof fw_resp, 0x03, data, 2, &n) == PN532_ERR_LEN_RANGE);

  CHECK_SUMMARY();
}
