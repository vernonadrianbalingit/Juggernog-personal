# Juggernog

[![CI](https://github.com/vernonadrianbalingit/Juggernog-personal/actions/workflows/ci.yml/badge.svg)](https://github.com/vernonadrianbalingit/Juggernog-personal/actions/workflows/ci.yml)

Juggernog is a drink dispenser I'm building that you control over the network.
The plan is an ESP32 running a pump: tap an NFC card, it sends a request over
Wi-Fi, and it pours.

Right now I'm building the networking side on my laptop first. The client and
server talk using Pourtocol, a small binary protocol I wrote on top of UDP in
C++20. There are no libraries, just POSIX sockets.

## Progress

- [x] Phase 1: UDP echo server/client
- [x] Phase 2: binary frame format with CRC-32
- [ ] Phase 3: reliability (ACKs, retransmit with exponential backoff, duplicate detection)
- [ ] Later: move it onto the ESP32 with FreeRTOS and hook up the pump

## Why UDP and not TCP?

The messages are tiny and the slow part is the pump, not the network. I want
short timeouts I can tune and memory use I can predict on a microcontroller,
and TCP doesn't really give me either. So I'm using UDP and adding only the
reliability I actually need. That's also the main thing I wanted to learn from
this project.

## Frame format

Everything multi-byte is big-endian.

```
 0       2     3      4              8        10           10+LEN
 +-------+-----+------+--------------+--------+--- ... ---+--------+
 | MAGIC | VER | TYPE |   SEQUENCE   |  LEN   |  PAYLOAD  | CRC-32 |
 |  (2)  | (1) | (1)  |     (4)      |  (2)   |  (LEN)    |  (4)   |
 +-------+-----+------+--------------+--------+--- ... ---+--------+
```

- **MAGIC**: always `0x50 0x43` ("PC"). Anything else sent to the port gets dropped right away.
- **VER**: protocol version, currently 1.
- **TYPE**: PING `0x01`, PONG `0x02`, DISPENSE_REQ `0x10`, DISPENSE_ACK `0x11`, STATUS_REQ `0x20`, STATUS_RES `0x21`.
- **SEQUENCE**: used to match replies to requests (and for duplicate detection in phase 3).
- **LEN**: payload length, max 1024.
- **CRC-32**: same polynomial Ethernet uses, computed over everything before it.

The header plus CRC is 14 bytes, so an empty PING is 14 bytes on the wire.

The parser checks things in this order and returns a different error code for
each: `TOO_SHORT`, `BAD_MAGIC`, `BAD_VERSION`, `LENGTH_MISMATCH`, `BAD_CRC`. It
has to check the header first, because it doesn't know where the CRC is until
it's read the length field. Fields are read with `memcpy` instead of casting
pointers, since unaligned reads can crash on some ARM chips.

## Files

| File | |
|---|---|
| `frame.h/.cpp` | Frame struct, `serialize()`, `parse()` |
| `crc32.h/.cpp` | table-based CRC-32 |
| `server.cpp` | checks incoming frames, replies to PING with PONG |
| `client.cpp` | sends a PING (optionally with a payload) and prints the reply |
| `test_frame.cpp` | unit tests |

## Building

Needs clang++ (C++20) on macOS or Linux.

```bash
make
make test
```

Run the server in one terminal and the client in another:

```bash
./server
./client 127.0.0.1
./client 127.0.0.1 "hello"
```

```
Sending PING (14 bytes on the wire)...
Got reply: type=0x02 seq=1 payload=0 bytes
```

If you send it something that isn't a Pourtocol frame, the server rejects it
and prints why:

```bash
echo "random garbage" | nc -u 127.0.0.1 9999
# Rejected packet from 127.0.0.1:54321 (15 bytes): BAD_MAGIC
```

## Tests

`make test` covers round trips (with and without a payload), each way a frame
can be rejected, and the standard CRC-32 check value
(`"123456789"` -> `0xCBF43926`). CI runs the tests on every push.

<!-- TODO: add wireshark screenshots
![PING frame in Wireshark](docs/wireshark-ping.png)
![CRC rejecting a corrupted frame](docs/crc-reject.png)
-->

## Known issues / TODO

- No reliability yet. If a packet gets lost, the client just waits on
  `recvfrom` forever. Phase 3 fixes this.
- The client always sends `seq=1` for now.
- The CRC only catches corruption. It doesn't stop someone from forging a frame
  (there's no authentication).
- IPv4 only, and the server is single-threaded.
