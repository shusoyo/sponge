#include "tcp_connection.hh"

#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "tcp_state.hh"

#include <bits/types/wint_t.h>
#include <exception>
#include <iostream>
#include <optional>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using std::cerr, std::string, std::exception;

size_t TCPConnection::remaining_outbound_capacity() const {
  return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
  return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
  return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
  return ms_since_last_receive_seg_;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
  // reset the value when receiving a new segment
  ms_since_last_receive_seg_ = 0;

  // BUT i don't know the mean of 'kill the connection'
  // TODO: kill the connection
  //  if the rst (reset) flag is set, sets both the inbound and outbound streams
  //  to the error state and kills the connection permanently. Otherwise it. . .
  if (seg.header().rst == true) {
    unclean_shutdown();
    return;
  }

  //  gives the segment to the TCPReceiver so it can inspect the fields it cares
  //  about on incoming segments: seqno, syn , payload, and fin .
  _receiver.segment_received(seg);

  // if the ack flag is set, tells the TCPSender about the fields it cares about
  // on incoming segments: ackno and window size.
  if (seg.header().ack == true) {
    _sender.ack_received(seg.header().ackno, seg.header().win);
  }

  if (TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED &&
      TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV) {
    connect();
    return;
  }

  // if the incoming segment occupied any sequence numbers, the TCPConnection
  // makes sure that at least one segment is sent in reply, to reflect an update
  // in the ackno and window size.
  // (If _sender.segments_out() have segment not send, it satisify the require)
  if (seg.length_in_sequence_space() > 0 && _sender.segments_out().empty()) {
    _sender.send_empty_segment();
  }

  // There is one extra special case that you will have to handle in the
  // TCPConnection’s segment received() method: responding to a “keep-alive”
  // segment. The peer may choose to send a segment with an invalid sequence
  // number to see if your TCP imple- mentation is still alive (and if so, what
  // your current window is). Your TCPConnection should reply to these
  // “keep-alives” even though they do not occupy any sequence numbers.
  if (_receiver.ackno().has_value() and
      (seg.length_in_sequence_space() == 0) and
      seg.header().seqno == _receiver.ackno().value() - 1) {
    _sender.send_empty_segment();
  }

  send_segments();
  clean_shutdown();
}

bool TCPConnection::active() const {
  return !is_closed;
}

size_t TCPConnection::write(const string &data) {
  auto write_size = _sender.stream_in().write(data);

  _sender.fill_window();
  send_segments();

  return write_size;
}

// any time the TCPSender has pushed a segment onto its outgoing queue,
// having set the fields it’s responsible for on outgoing segments: (seqno, syn
// , payload, and fin ).
//
// Before sending the segment, the TCPConnection will
// ask the TCPReceiver for the fields it’s responsible for on outgoing segments:
// ackno and window size. If there is an ackno, it will set the ack flag and the
// fields in the TCPSegment.
void TCPConnection::send_segments() {
  auto &sender_seg_queue = _sender.segments_out();
  while (not sender_seg_queue.empty()) {
    auto seg = sender_seg_queue.front();
    sender_seg_queue.pop();

    seg.header().win = _receiver.window_size();

    if (_receiver.ackno().has_value()) {
      seg.header().ack = true;
      seg.header().ackno = _receiver.ackno().value();
    }

    _segments_out.push(seg);
  }
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
  // maintain the receive_segment_gap_time
  ms_since_last_receive_seg_ += ms_since_last_tick;

  // tick the send timer
  _sender.tick(ms_since_last_tick);

  // abort the connection, and send a reset segment to the peer (an empty
  // segment with the rst flag set), if the number of consecutive
  // retransmissions is more than an upper limit TCPConfig::MAX RETX ATTEMPTS.

  if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
    // abort the connection
    unclean_shutdown();

    // send the reset segment
    send_reset_segment();

    return;
  }

  send_segments();
  clean_shutdown();
}

void TCPConnection::end_input_stream() {
  _sender.stream_in().end_input();
  _sender.fill_window();
  send_segments();
}

void TCPConnection::connect() {
  _sender.fill_window();
  send_segments();
}

// private function {
void TCPConnection::send_reset_segment() {
  // seg with rst flag don't need guarantee acked
  TCPSegment seg {};
  seg.header().rst = true;
  _segments_out.emplace(seg);
}

// In an unclean shutdown, the TCPConnection either sends or receives a segment
// with the rst flag set. In this case, the outbound and inbound ByteStreams
// should both be in the error state, and active() can return false immediately.
void TCPConnection::unclean_shutdown() {
  _sender.stream_in().set_error();
  _receiver.stream_out().set_error();
  is_closed = true;
}

void TCPConnection::clean_shutdown() {
  // Prereq #1 The inbound stream has been fully assembled and has ended.

  // Prereq #2 The outbound stream has been ended by the local application and
  // fully sent (including the fact that it ended, i.e. a segment with fin ) to
  // the remote peer.

  // Prereq #3 The outbound stream has been fully acknowledged by the remote
  // peer.

  // Prereq #4 The local TCPConnection is confident that the remote peer can
  // satisfy prerequisite #3. This is the brain-bending part. There are two
  // alternative ways this can happen:

  // passive close
  if (_receiver.stream_out().input_ended() and !_sender.stream_in().eof()) {
    _linger_after_streams_finish = false;
  }

  //  lingering
  if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
      TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED) {
    if (_linger_after_streams_finish == false or
        ms_since_last_receive_seg_ >= 10 * _cfg.rt_timeout) {
      is_closed = true;
    }
  }
}
// }

TCPConnection::~TCPConnection() {
  try {
    if (active()) {
      cerr << "Warning: Unclean shutdown of TCPConnection\n";

      send_reset_segment();
      unclean_shutdown();
      // Your code here: need to send a RST segment to the peer
    }
  } catch (const exception &e) {
    std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
  }
}
