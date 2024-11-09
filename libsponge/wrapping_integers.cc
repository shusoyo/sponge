#include "wrapping_integers.hh"

#include <cstdint>
#include <limits>
#include <sys/types.h>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

constexpr uint64_t MOD = 1ll << 32;
//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a
//! WrappingInt32 \param n The input absolute 64-bit sequence number \param isn
//! The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
  return WrappingInt32 { static_cast<uint32_t>(
    (static_cast<uint64_t>(isn.raw_value()) + n % MOD) % MOD) };
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number
//! (zero-indexed) \param n The relative sequence number \param isn The initial
//! sequence number \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to
//! `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One
//! stream runs from the local TCPSender to the remote TCPReceiver and has one
//! ISN, and the other stream runs from the remote TCPSender to the local
//! TCPReceiver and has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
  uint64_t x = n.raw_value() - isn.raw_value();
  if (checkpoint <= x) {
    return x;
  } else {
    x += MOD * ((checkpoint - x) / MOD);
    if (x > std::numeric_limits<uint64_t>::max() - MOD) {
      return x;
    } else {
      uint64_t left = x, right = x + MOD;
      return checkpoint - left > right - checkpoint ? right : left;
    }
  }
}
