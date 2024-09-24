#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <cstdint>
// #include <functional>
// #include <map>
#include <iostream>
#include <optional>
#include <queue>

struct OutstandingSeg {
  uint64_t end_seq;
  TCPSegment seg;

  OutstandingSeg(uint64_t _seq, TCPSegment _seg)
    : end_seq(_seq + _seg.length_in_sequence_space()), seg(_seg) {}
};

class Retransmission {
private:
  bool _timer_state {};
  unsigned int _retransmission_timeout {};
  unsigned int _time {};

  unsigned int _consecutive_retransmissions {};

  uint64_t _bytes_in_flight { 0 };

  std::queue<OutstandingSeg> _outstandings {};

  std::queue<TCPSegment> &_segments_out;

  bool _finished {};

public:
  Retransmission(std::queue<TCPSegment> &out);
  void check(const size_t duration, uint64_t window_size);

  void sweep(const uint64_t ack_no);

  void start_timer(unsigned int retransmission_timeout) {
    if (not _timer_state) {
      _timer_state = true;
      _retransmission_timeout = retransmission_timeout;
      _consecutive_retransmissions = 0;
    }
  }

  void restart_timer(unsigned int retransmission_timeout) {
    _timer_state = not _outstandings.empty();
    _time = 0;
    _retransmission_timeout = retransmission_timeout;
    _consecutive_retransmissions = 0;
  }

  void add(const OutstandingSeg &out_seg) {
    _outstandings.push(out_seg);
    _bytes_in_flight += out_seg.seg.length_in_sequence_space();
  }

  uint64_t bytes_in_flight() const {
    return _bytes_in_flight;
  }

  unsigned int consecutive_retransmissions() const {
    return _consecutive_retransmissions;
  }
};

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
private:
  //! our initial sequence number, the number for our SYN.
  WrappingInt32 _isn;

  //! outbound queue of segments that the TCPSender wants sent
  std::queue<TCPSegment> _segments_out {};

  //! retransmission timer for the connection
  unsigned int _initial_retransmission_timeout;

  //! outgoing stream of bytes that have not yet been sent
  ByteStream _stream;

  //! the (absolute) sequence number for the next byte to be sent
  uint64_t _next_seqno { 0 };

  //! ack_no
  uint64_t _ack_no {};

  //! receiver window size
  uint64_t _window_size { 1 };

  Retransmission _resender;

  std::optional<uint64_t> _fin_seq {};

public:
  //! Initialize a TCPSender
  TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
            const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
            const std::optional<WrappingInt32> fixed_isn = {});

  //! \name "Input" interface for the writer
  //!@{
  ByteStream &stream_in() {
    return _stream;
  }
  const ByteStream &stream_in() const {
    return _stream;
  }
  //!@}

  //! \name Methods that can cause the TCPSender to send a segment
  //!@{

  //! \brief A new acknowledgment was received
  void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

  //! \brief Generate an empty-payload segment (useful for creating empty ACK
  //! segments)
  void send_empty_segment();

  //! \brief create and send segments to fill as much of the window as possible
  void fill_window();

  //! \brief Notifies the TCPSender of the passage of time
  void tick(const size_t ms_since_last_tick);
  //!@}

  //! \name Accessors
  //!@{

  //! \brief How many sequence numbers are occupied by segments sent but not yet
  //! acknowledged? \note count is in "sequence space," i.e. SYN and FIN each
  //! count for one byte (see TCPSegment::length_in_sequence_space())
  size_t bytes_in_flight() const;

  //! \brief Number of consecutive retransmissions that have occurred in a row
  unsigned int consecutive_retransmissions() const;

  //! \brief TCPSegments that the TCPSender has enqueued for transmission.
  //! \note These must be dequeued and sent by the TCPConnection,
  //! which will need to fill in the fields that are set by the TCPReceiver
  //! (ackno and window size) before sending.
  std::queue<TCPSegment> &segments_out() {
    return _segments_out;
  }
  //!@}

  //! \name What is the next sequence number? (used for testing)
  //!@{

  //! \brief absolute seqno for the next byte to be sent
  uint64_t next_seqno_absolute() const {
    return _next_seqno;
  }

  //! \brief relative seqno for the next byte to be sent
  WrappingInt32 next_seqno() const {
    return wrap(_next_seqno, _isn);
  }

  bool finished() const {
    return _fin_seq.has_value() and _fin_seq.value() < _ack_no;
  }

  //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
