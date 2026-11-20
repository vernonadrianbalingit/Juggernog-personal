#include "pn532_frame.h"

size_t pn532_build_frame(uint8_t *out, const uint8_t *data, uint8_t n) {
  if (n == 0 || n > PN532_MAX_DATA) {
    return 0;
  }
  uint8_t len = (uint8_t)(n + 1);     //TFI + data
  uint8_t sum = PN532_TFI_HOST_TO_PN532;
  size_t i = 0;

  out[i++] = 0x00;
  out[i++] = 0x00;
  out[i++] = 0xFF;
  out[i++] = len;
  out[i++] = (uint8_t)(~len + 1);     //LCS, twos complement so LEN + LCS wraps to 0
  out[i++] = PN532_TFI_HOST_TO_PN532;
  for (uint8_t k = 0; k < n; k++) {
    out[i++] = data[k];
    sum = (uint8_t)(sum + data[k]);   //keep a running total for the DCS
  }
  out[i++] = (uint8_t)(~sum + 1);     //DCS, same trick over TFI + DATA
  out[i++] = 0x00;
  return i;
}

pn532_parse_t pn532_parse_response(const uint8_t *buf, size_t len, uint8_t expect_cmd,
                                   uint8_t *out, size_t out_cap, uint8_t *out_len) {
  //1. smallest frame that can exist: 00 00 FF LEN LCS TFI CMD DCS = 8 bytes
  if (len < 8) {
    return PN532_ERR_TOO_SHORT;
  }
  //2. has to start with the preamble
  if (buf[0] != 0x00 || buf[1] != 0x00 || buf[2] != 0xFF) {
    return PN532_ERR_PREAMBLE;
  }
  //3. LEN has its own checksum, check that before trusting LEN for anything
  uint8_t flen = buf[3];
  if ((uint8_t)(flen + buf[4]) != 0) {
    return PN532_ERR_LEN_CHECKSUM;
  }
  //4. bounds check BEFORE touching any data. LEN has to cover at least TFI + CMD,
  //   and the whole frame (5 header bytes + LEN + DCS) has to fit in what i was given
  if (flen < 2 || (size_t)flen + 6 > len) {
    return PN532_ERR_LEN_RANGE;
  }
  //5. direction, only accept frames coming from the PN532
  if (buf[5] != PN532_TFI_PN532_TO_HOST) {
    return PN532_ERR_TFI;
  }
  //6. data checksum, TFI + DATA + DCS should wrap to 0
  uint8_t sum = 0;
  for (size_t i = 0; i < (size_t)flen + 1; i++) {
    sum = (uint8_t)(sum + buf[5 + i]);
  }
  if (sum != 0) {
    return PN532_ERR_DATA_CHECKSUM;
  }
  //7. is this the reply to the command i actually sent?
  if (buf[6] != expect_cmd) {
    return PN532_ERR_COMMAND;
  }
  //8. everything passed, now its safe to copy out
  uint8_t n = (uint8_t)(flen - 2);    //minus TFI and CMD
  if (n > out_cap) {
    return PN532_ERR_LEN_RANGE;
  }
  for (uint8_t i = 0; i < n; i++) {
    out[i] = buf[7 + i];
  }
  *out_len = n;
  return PN532_OK;
}

const char *pn532_parse_str(pn532_parse_t r) {
  switch (r) {
    case PN532_OK:                return "OK";
    case PN532_ERR_TOO_SHORT:     return "TOO_SHORT";
    case PN532_ERR_PREAMBLE:      return "BAD_PREAMBLE";
    case PN532_ERR_LEN_CHECKSUM:  return "BAD_LEN_CHECKSUM";
    case PN532_ERR_LEN_RANGE:     return "BAD_LEN_RANGE";
    case PN532_ERR_TFI:           return "BAD_TFI";
    case PN532_ERR_COMMAND:       return "WRONG_COMMAND";
    case PN532_ERR_DATA_CHECKSUM: return "BAD_DATA_CHECKSUM";
  }
  return "UNKNOWN";
}
