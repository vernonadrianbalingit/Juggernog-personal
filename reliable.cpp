#include "reliable.h"

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <random>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace juggernog {

namespace {

constexpr size_t RECV_BUF_SIZE = 2048;

double g_loss_rate = 0.0;

std::mt19937& rng() { 
  //seed once on first use 
  static std::mt19937 engine(std::random_device{}());
  return engine;
}

bool should_drop() {
  if (g_loss_rate <= 0.0) return false;
  std::uniform_real_distribution<double> dist(0.0,1.0);
  return dist(rng()) < g_loss_rate;
}


using Clock = std::chrono::steady_clock;

int elapsed_ms(Clock::time_point from, Clock::time_point to) { 
  return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(to-from).count());
}

} //anonymous namespace 

void set_lossrate(double p) { 
  g_loss_rate = std::clamp(p, 0.0, 1.0);
}

double loss_rate(){
  return g_loss_rate;
}

ssize_t lossy_sendto(int sock, const void* buf, size_t len, 
                    const sockaddr_in& dest){
  if(should_drop()) { 
    //Pretend the kernel azccepted it, the successor sendto() doens't mean "delievered"
    // it means "queued". sim the drop here
    return static_cast<ssize_t>(len);
  }
  return sendto(sock, buf, len, 0, reinterpret_cast<const sockaddr*>(&dest), sizeof(dest));

}

int next_timeout_ms(int current_ms, const RetryConfig& config) { 
  const long doubled = static_cast<long>(current_ms) * 2;
  if (doubled >= static_cast<long>(config.max_timeout_ms)) { 
    return config.max_timeout_ms;
  }
  return static_cast<int>(doubled);
}

int apply_jitter(int timeout_ms, double jitter) { 
  if(jitter <= 0.0) return timeout_ms;
  std::uniform_real_distribution<double> dist(-jitter, jitter);
  const double scaled = timeout_ms * (1.0 + dist(rng()));
  return std::max(1, static_cast<int>(scaled));
}

bool send_reliable(int sock,
                  const Frame& frame,
                  const sockaddr_in& dest,
                  const RetryConfig& config,
                  Frame& reply_out,
                  SendStats& stats) { 

  const std::vector<uint8_t> wire = serialize(frame);

  stats = SendStats{};
  int timeout_ms = config.initial_timeout_ms;

  for (int attempt = 0; attempt <= config.max_retries; ++attempt) { 
    const Clock::time_point sent_at = Clock::now();

    if (lossy_sendto(sock, wire.data(), wire.size(), dest) < 0) { 
      return false; //real socket error not just a loss 
    }
    stats.transmissions++;

    const int budget_ms = apply_jitter(timeout_ms, config.jitter);
    const auto deadline = sent_at + std::chrono::milliseconds(budget_ms);

    //inner loop: keep waiting until the deadline. 
    //may wake for packets that are not the ACK we want (junk, stale)
    //those cannot consume the whole timeout 

    for(;;) { 
      const Clock::time_point now = Clock::now();
      if (now >= deadline) break; //timed out -> retransmit 

      pollfd pfd{};
      pfd.fd = sock;
      pfd.events = POLLIN;
      
      const int ready = poll(&pfd, 1, elapsed_ms(now, deadline));
      if (ready < 0) { 
        if(errno == EINTR) continue; //signal, not an error
        return false;
      }
      if (ready == 0) break; //timed out -> retransmit 

      uint8_t buf[RECV_BUF_SIZE];
      sockaddr_in from{};
      socklen_t from_len = sizeof(from);

      const ssize_t n = recvfrom(sock, buf, RECV_BUF_SIZE, 0, reinterpret_cast<sockaddr*>(&from),
                                &from_len);
      if (n < 0) { 
        if(errno == EINTR) continue;
        return false;
      }

      Frame reply{};
      if (parse(buf, static_cast<size_t>(n), reply) != ParseResult::OK) { 
        continue; //corrupt or foreign, ignore
      }
      if (reply.sequence != frame.sequence) { 
        continue;  //an ACK buts not ours 
      }

      stats.acked = true;
      //Karn/Patridge is where an ack following a retransmission is abiguous 
      //it is not a usable RTT sample, we can only measure a first clean first try
      stats.rtt_ms = (stats.transmissions == 1) 
                    ? elapsed_ms(sent_at, Clock::now()) : -1;
      reply_out = reply; 
      return true;
    }

    timeout_ms = next_timeout_ms(timeout_ms, config);
  }

  return false;  //retry budget maxed out 
}

} //namespace juggernog