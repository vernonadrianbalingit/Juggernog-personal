#include "dedupe.h"

namespace juggernog { 

  uint64_t DuplicateFilter::key_for(const sockaddr_in& peer) { 
    //pack the 32 bit address and 16 bit port into one integer key. 
    // convert out of network byte order first so the key is stable 
    // disgregardding host endianness 

    const uint64_t ip = static_cast<uint64_t>(ntohl(peer.sin_addr.s_addr));
    const uint64_t port = static_cast<uint64_t>(ntohs(peer.sin_port));
    return (ip << 16) | port;
  }

  DuplicateFilter::Verdict DuplicateFilter::check(const sockaddr_in& peer, uint32_t sequence) { 
    const uint64_t key = key_for(peer);
    auto it = last_seen_.find(key); 

    if (it == last_seen_.end()) { 
      last_seen_.emplace(key, sequence);
      return Verdict::NEW;
    }

    if (sequence == it->second) { 
      return Verdict::DUPLICATE;
    }
    if (sequence < it->second) { 
      return Verdict::STALE;
    }

    it->second = sequence;
    return Verdict::NEW;
  }

  void DuplicateFilter::reset(){
    last_seen_.clear();
  }

  const char* verdict_to_str(DuplicateFilter::Verdict v){
    switch(v) {
      case DuplicateFilter::Verdict::NEW: return "NEW";
      case DuplicateFilter::Verdict::DUPLICATE: return "DUPLICATE";
      case DuplicateFilter::Verdict::STALE: return "STALE";
    }
    return "UNKNOWN";
  }
} //namespace juggernog