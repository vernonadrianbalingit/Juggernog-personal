/*
reliable.h for stop and wait ARQ over UDP 


send_reliable() transmits a frame and blocks until a frame with the same 
seq number comes back, or the retry budget is exhausted 

retransmission timeouts double after each failure (exponential backoff)
with random jitter to avoid sync retry storms 
*/

#pragma once

#include "frame.h"

#include <cstdint>
#include <cstddef>
#include <netinet/in.h> 
#include <sys/types.h>

namespace juggernog {

struct RetryConfig { 
  int max_retries = 5; //retransmitions after first send 
  int initial_timeout_ms = 200;
  int max_timeout_ms = 3200; //ceiling so wait doesn't get too big 
  double jitter = 0.25;   //plus or minus: fraction applied to each timeout. 
};

struct SendStats { 
  int transmissions = 0; // 1 is where it worked first try
  int rtt_ms = -1; //-1 means no valid sample 
  bool acked = false; //did we get a ack back? 
};

//Send 'frame' to 'dest', retransmit until a frame with same seq# arrives or retry budget(5) 
//Runs out. Return true on success and fill out reply_out

bool send_reliable(int sock,  
                  const Frame& frame,
                  const sockaddr_in& dest,
                  const RetryConfig& config,
                  Frame& reply_out,
                  SendStats& stats);


// --- Backoff math so tests can check them without sockets

// Double 'current_ms', clamped to config.max_timeout_ms
int next_timeout_ms(int current_ms, const RetryConfig& config);

//Scale 'timeout_ms' with a random factgor in [1-jitter, 1_jitter]
int apply_jitter(int timeout_ms, double jitter);

// -- Fault Injection (test hook) ----

// Probability in [0,1] if an outbound datagram is discarded instead of sent, default 0 
void set_lossrate(double p);
double loss_rate();

//sendto() 
ssize_t lossy_sendto(int sock,
                    const void* buf,
                    size_t len, 
                    const sockaddr_in& dest);
} //namespace juggernog