/* 
dedupe.h - recuiver side duplication detection 

SEQ nums are unique per sender, NOT globally. Filter keys on
source IP, source port) so each peer gets one slot holding the highest 
SEQ num we've processed
*/

#pragma once

#include <cstdint>
#include <cstddef> 
#include <unordered_map> 
#include <netinet/in.h> 

namespace juggernog { 
  
  class DuplicateFilter {
  public: 
    enum class Verdict { 
      NEW, //first time seeing the sequence from peer , ACT on it 
      DUPLICATE, //exact Retransmission as last one -> re-ACK and DO NOT ACT 
      STALE, //older than we processed already -> re ACK do not ACT  
    };

    //Record and classify an incoming sequence number from "peer".
    Verdict check(const sockaddr_in& peer, uint32_t sequence); 

    //Forget all peers, test hook
    void reset();

    size_t tracked_peers() const { return last_seen_.size(); } 

  private: 
    static uint64_t key_for(const sockaddr_in& peer);

    //key = (ipv4 << 16) | port
    std::unordered_map<uint64_t, uint32_t> last_seen_;
  };

  const char* verdict_to_str(DuplicateFilter::Verdict v);

} //namespace juggernog
