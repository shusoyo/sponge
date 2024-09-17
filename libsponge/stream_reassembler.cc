#include "stream_reassembler.hh"
#include <cstddef>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _byte_pending(), next_byte(), buffer(), end_index(), is_get_end() {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const std::string &data, const size_t index, const bool eof) {
  using i64 = long long;
  i64 difference = next_byte - index;

  if (difference == 0 or (difference > 0 and difference < i64(data.size()))) {
    std::string_view view(data);
    view = view.substr(difference, std::min(_output.remaining_capacity(), data.size() - size_t(difference)));

    for (size_t i = 0; i < view.size() && next_byte + i < buffer.size(); i++) {
      if (buffer[next_byte + i].second) {
        _byte_pending -= 1;
      }
    }

    std::string will_write {view};
    next_byte += view.size();

    for (size_t i = next_byte; i < buffer.size(); i++) {
      const auto &[value, state] = buffer[i];
      if (state == false) {
        break;
      }

      will_write += value;
      next_byte += 1;
      _byte_pending -= 1;
    }

    _output.write(will_write);
  }

  if (difference < 0 and _output.remaining_capacity() + next_byte > index) {
    if (buffer.size() < index + data.size()) {
      buffer.resize(index + data.size() + 1);
    }

    size_t length = std::min(data.size(), _output.remaining_capacity() + next_byte - index);

    for (size_t i = 0; i < length; i++) {
      auto &[value, state] = buffer[i + index];
      value = data[i];

      if (state) {
        _byte_pending -= 1;
      }

      state = true;
    }

    _byte_pending += length;
  }

  if (eof) {
    end_index = index + data.size() - 1;
    is_get_end = true;
  }

  if (is_get_end && next_byte - 1 == end_index) {
    _output.end_input();
  }
}

size_t StreamReassembler::unassembled_bytes() const {
  return _byte_pending;
}

bool StreamReassembler::empty() const {
  return _byte_pending == 0;
}

size_t StreamReassembler::available_capacity() const {
  return _output.remaining_capacity() - _byte_pending;
}