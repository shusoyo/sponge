#include "tcp_receiver.hh"

#include "wrapping_integers.hh"

#include <cstdint>
#include <optional>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

void TCPReceiver::segment_received(const TCPSegment &seg) {
  if (seg.header().syn) {
    isn = seg.header().seqno;
    is_start = true;
  }

  uint64_t index =
    unwrap(seg.header().seqno + seg.header().syn, isn, reassembler_.checkpoint()) - 1;
  reassembler_.push_substring(seg.payload().copy(), index, seg.header().fin);
}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
  if (is_start) {
    return { wrap(reassembler_.checkpoint(), isn) + is_start +
             reassembler_.stream_out().input_ended() };
  } else {
    return std::nullopt;
  }
}

size_t TCPReceiver::window_size() const {
  return reassembler_.stream_out().remaining_capacity();
}
