#pragma once
#include <stddef.h>
#include <stdint.h>

//PN532 frame build + parse. Pure C, no hardware, so the host tests can run it on my mac
//
//  00 00 FF | LEN | LCS | TFI | DATA ... | DCS | 00
//  LEN = how many bytes in TFI + DATA      LCS: LEN + LCS = 0 (mod 256)
//  TFI = D4 me -> PN532, D5 PN532 -> me    DCS: TFI + DATA + DCS = 0 (mod 256)

#define PN532_TFI_HOST_TO_PN532 0xD4
#define PN532_TFI_PN532_TO_HOST 0xD5
#define PN532_MAX_DATA          32  //longest command or response i use
#define PN532_FRAME_OVERHEAD    8   //every byte that wraps around DATA

typedef enum {
  PN532_OK = 0,
  PN532_ERR_TOO_SHORT,      //less bytes than the smallest frame possible
  PN532_ERR_PREAMBLE,       //does not start with 00 00 FF
  PN532_ERR_LEN_CHECKSUM,   //LEN + LCS != 0
  PN532_ERR_LEN_RANGE,      //LEN says more bytes than i actually have, or too many
  PN532_ERR_TFI,            //not a PN532 -> host frame
  PN532_ERR_COMMAND,        //reply to a different command than the one i sent
  PN532_ERR_DATA_CHECKSUM,  //TFI + DATA + DCS != 0
} pn532_parse_t;

//data[0] is the command byte. Returns how many bytes got written to out,
//or 0 if n is 0 or too big. out needs room for n + PN532_FRAME_OVERHEAD bytes
size_t pn532_build_frame(uint8_t *out, const uint8_t *data, uint8_t n);

//buf has to point at the 00 00 FF preamble (skip the i2c status byte first)
//On PN532_OK, copies the bytes AFTER the response command byte into out
pn532_parse_t pn532_parse_response(const uint8_t *buf, size_t len, uint8_t expect_cmd,
                                   uint8_t *out, size_t out_cap, uint8_t *out_len);

const char *pn532_parse_str(pn532_parse_t r);
