#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Read-only V1.03 transaction. No arbitrary TX or persistent-setting commands.
// Source: docs/LD2451-product-sheet.pdf pp. 4–8; see protocol notes for errata.
namespace radar_query {
class Engine {
 public:
  using Send = void (*)(const uint8_t*, size_t);
  using Event = void (*)(const char*);
  using Ack = void (*)(const uint8_t*, size_t);
  Engine(Send send, Event event, Ack ack) : send_(send), event_(event), ack_(ack) {}
  bool active() const { return stage_ != 0; }
  bool start(uint32_t now) {
    if (active()) { event_("ERROR query_already_running"); return false; }
    used_ = 0; failed_ = false; stage_ = 1;
    event_("QUERY_STARTED"); request(now); return true;
  }
  void cancel(uint32_t now) {
    if (active()) fail(now, "ERROR query_cancelled");
  }
  void tick(uint32_t now) {
    if (active() && uint32_t(now - sentAt_) >= 2000)
      fail(now, "ERROR ack_timeout");
  }
  void feed(uint8_t byte, uint32_t now) {
    if (used_ == sizeof(buffer_)) drop(1);
    buffer_[used_++] = byte;
    while (used_ >= 4) {
      const uint8_t header[] = {0xfd, 0xfc, 0xfb, 0xfa};
      if (memcmp(buffer_, header, 4)) { drop(1); continue; }
      if (used_ < 6) return;
      const size_t length = buffer_[4] | (size_t(buffer_[5]) << 8);
      if (length < 4 || length > 64) { drop(1); continue; }
      if (used_ < length + 10) return;
      const uint8_t trailer[] = {4, 3, 2, 1};
      if (memcmp(buffer_ + 6 + length, trailer, 4)) { drop(1); continue; }
      ack_(buffer_, length + 10); // Preserve every structurally valid ACK.
      const uint16_t command = buffer_[6] | (uint16_t(buffer_[7]) << 8);
      const uint16_t status = buffer_[8] | (uint16_t(buffer_[9]) << 8);
      drop(length + 10);
      if (!active() || command != (commands_[stage_-1] | 0x0100)) continue;
      if (status) { fail(now, "ERROR ack_status"); continue; }
      const size_t expected[] = {8, 8, 8, 12, 4};
      if (length != expected[stage_-1]) { fail(now, "ERROR ack_shape"); continue; }
      if (stage_ == 5) finish(true);
      else { ++stage_; request(now); }
    }
  }
 private:
  Send send_; Event event_; Ack ack_;
  uint8_t buffer_[80]{};
  size_t used_ = 0;
  unsigned stage_ = 0;
  bool failed_ = false;
  uint32_t sentAt_ = 0;
  const uint16_t commands_[5] = {0xff, 0x12, 0x13, 0xa0, 0xfe};
  void drop(size_t n) { used_ -= n; memmove(buffer_, buffer_ + n, used_); }
  void request(uint32_t now) {
    uint8_t packet[14] = {0xfd,0xfc,0xfb,0xfa,2,0,0,0,4,3,2,1};
    packet[6] = uint8_t(commands_[stage_-1]);
    size_t size = 12;
    if (stage_ == 1) {
      packet[4] = 4; packet[8] = 1; packet[9] = 0;
      packet[10] = 4; packet[11] = 3; packet[12] = 2; packet[13] = 1;
      size = 14;
    }
    sentAt_ = now; send_(packet, size);
  }
  void fail(uint32_t now, const char* message) {
    event_(message); failed_ = true;
    if (stage_ == 5) finish(false);
    else { stage_ = 5; request(now); } // Attempt exit even if enter ACK was lost.
  }
  void finish(bool exitConfirmed) {
    stage_ = 0;
    event_(exitConfirmed ? "CONFIG_EXIT_CONFIRMED" : "CONFIG_EXIT_UNCONFIRMED");
    event_(failed_ ? "QUERY_FAILED" : "QUERY_COMPLETE");
  }
};
} // namespace radar_query
