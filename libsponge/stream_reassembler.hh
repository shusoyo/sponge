#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

using std::optional, std::vector;

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
private:
  // Your code here -- add private members as necessary.

  ByteStream output_;  //!< The reassembled in-order byte stream
  size_t capacity_;    //!< The maximum number of bytes
  size_t byte_pending_ {};
  size_t next_byte_ {};

  // the first char is a byte, the second char is bool
  vector<optional<char>> buffer_ {};
  int64_t end_index_ {};
  bool is_get_end_ {};

public:
  //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
  //! \note This capacity limits both the bytes that have been reassembled,
  //! and those that have not yet been reassembled.
  StreamReassembler(const size_t capacity);

  //! \brief Receive a substring and write any newly contiguous bytes into the stream.
  //!
  //! The StreamReassembler will stay within the memory limits of the `capacity`.
  //! Bytes that would exceed the capacity are silently discarded.
  //!
  //! \param data the substring
  //! \param index indicates the index (place in sequence) of the first byte in `data`
  //! \param eof the last byte of `data` will be the last byte in the entire stream
  void push_substring(const std::string &data,
                      const uint64_t index,
                      const bool eof);

  //! \name Access the reassembled byte stream
  //!@{
  const ByteStream &stream_out() const {
    return output_;
  }
  ByteStream &stream_out() {
    return output_;
  }
  //!@}

  //! The number of bytes in the substrings stored but not yet reassembled
  //!
  //! \note If the byte at a particular index has been pushed more than once, it
  //! should only be counted once for the purpose of this function.
  size_t unassembled_bytes() const;

  //! \brief Is the internal state empty (other than the output stream)?
  //! \returns `true` if no substrings are waiting to be assembled
  bool empty() const;

  size_t available_capacity() const;

  uint64_t checkpoint() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
