#include "tcp_sender.hh"

#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before
//! retransmitting the oldest outstanding segment \param[in] fixed_isn the
//! Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity,
                     const uint16_t retx_timeout,
                     const std::optional<WrappingInt32> fixed_isn)
  : _isn(fixed_isn.value_or(WrappingInt32 { std::random_device()() }))
  , _initial_retransmission_timeout { retx_timeout }
  , _stream(capacity)
  , _resender(_segments_out) {}

uint64_t TCPSender::bytes_in_flight() const {
  return _resender.bytes_in_flight();
}

void TCPSender::fill_window() {
  if (finished()) {
    return;
  }

  auto send_seg = [&](TCPSegment &seg) {
    _segments_out.push(seg);
    _resender.add(OutstandingSeg(next_seqno_absolute(), seg));
    _next_seqno += seg.length_in_sequence_space();
    _resender.start_timer(_initial_retransmission_timeout);
  };

  if (_next_seqno == 0) {
    TCPSegment seg;
    seg.header().set_seqno(next_seqno()).set_syn(true);
    send_seg(seg);
  } else {
    uint64_t remain_window_size = _window_size == 0 ? 1 : _window_size;

    while (
      ((not _fin_seq.has_value() and _stream.eof()) or not _stream.buffer_empty()) and
      remain_window_size > bytes_in_flight()) {
      size_t length = std::min({ TCPConfig::MAX_PAYLOAD_SIZE,
                                 _stream.buffer_size(),
                                 remain_window_size - bytes_in_flight() });

      std::string payload = _stream.eof() ? "" : _stream.read(length);

      TCPSegment seg;
      seg.header().set_seqno(next_seqno());
      seg.payload() = std::move(payload);

      if (not _fin_seq.has_value() and _stream.eof() and
          (bytes_in_flight() + length + 1 <= remain_window_size)) {
        _fin_seq.emplace(_next_seqno + seg.length_in_sequence_space());
        seg.header().set_fin(true);
      }

      send_seg(seg);
    }
  }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
  uint64_t this_ack_no = unwrap(ackno, _isn, _ack_no);

  if (this_ack_no < _ack_no or this_ack_no > _next_seqno or
      (this_ack_no == _ack_no and window_size == _window_size) or
      (_fin_seq.has_value() and _ack_no == _fin_seq.value() and
       this_ack_no != _fin_seq.value() + 1) or
      finished()) {
    return;
  }

  _ack_no = this_ack_no;
  _window_size = window_size;

  _resender.sweep(_ack_no);
  _resender.restart_timer(_initial_retransmission_timeout);
  fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call
//! to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
  _resender.check(ms_since_last_tick, _window_size);
}

unsigned int TCPSender::consecutive_retransmissions() const {
  return _resender.consecutive_retransmissions();
}

void TCPSender::send_empty_segment() {
  TCPSegment seg;
  seg.header().set_seqno(next_seqno());
  _segments_out.push(seg);
}

Retransmission::Retransmission(std::queue<TCPSegment> &out) : segments_out_(out) {}

void Retransmission::check(const size_t duration, const uint64_t window_size) {
  if (not timer_state_) {
    return;
  }
  // std::cerr << window_size << "CALLED\n";

  time_ += duration;
  if (time_ >= retransmission_timeout_) {
    segments_out_.emplace(outstandings_.front().seg);
    time_ = 0;
    if (window_size > 0) {
      consecutive_retransmissions_ += 1;
      retransmission_timeout_ = retransmission_timeout_ * 2;
    }
  }
}

void Retransmission::sweep(const uint64_t ack_no) {
  while (not outstandings_.empty() and outstandings_.front().end_seq <= ack_no) {
    const auto &seg = outstandings_.front().seg;
    bytes_in_flight_ -= seg.length_in_sequence_space();

    outstandings_.pop();
  }
}
