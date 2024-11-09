#include "stream_reassembler.hh"

#include "buffer.hh"

#include <cstddef>
#include <cstdint>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in
// `stream_reassembler.hh`

using std::string, std::string_view;

StreamReassembler::StreamReassembler(const size_t capacity)
  : output_(capacity), capacity_(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data,
                                       const size_t index,
                                       const bool eof) {
  using i64 = long long;
  i64 difference = next_byte_ - index;

  // 1:                     2:
  //  next_byte               next_byte
  //     ↓                    ↓ ↓ ↓ ↓
  //     - - - -              - - - -
  //     ↑                    ↑
  //    index                 index
  if (difference == 0 or
      (difference > 0 and difference < static_cast<i64>(data.size()))) {
    string_view view(data);
    view = view.substr(
      difference,
      std::min(output_.remaining_capacity(), data.size() - size_t(difference)));

    for (size_t i = 0; i < view.size() && next_byte_ + i < buffer_.size();
         i++) {
      if (buffer_[next_byte_ + i].has_value()) {
        byte_pending_ -= 1;
      }
    }

    string will_write { view };
    next_byte_ += view.size();

    for (size_t i = next_byte_; i < buffer_.size(); i++) {
      if (not buffer_[i].has_value()) {
        break;
      }

      const auto value = buffer_[i].value();
      will_write += value;
      next_byte_ += 1;
      byte_pending_ -= 1;
    }

    output_.write(will_write);
  }

  if (difference < 0 and output_.remaining_capacity() + next_byte_ > index) {
    if (buffer_.size() < index + data.size()) {
      buffer_.resize(index + data.size() + 1);
    }

    size_t length =
      std::min(data.size(), output_.remaining_capacity() + next_byte_ - index);

    for (size_t i = 0; i < length; i++) {
      auto &chr = buffer_[i + index];

      if (chr.has_value()) {
        byte_pending_ -= 1;
      }

      chr.emplace(data[i]);
    }

    byte_pending_ += length;
  }

  if (eof) {
    end_index_ = index + data.size() - 1;
    is_get_end_ = true;
  }

  if (is_get_end_ && next_byte_ == static_cast<size_t>(end_index_ + 1)) {
    output_.end_input();
  }
}

size_t StreamReassembler::unassembled_bytes() const {
  return byte_pending_;
}

bool StreamReassembler::empty() const {
  return byte_pending_ == 0;
}

size_t StreamReassembler::available_capacity() const {
  return output_.remaining_capacity() - byte_pending_;
}

uint64_t StreamReassembler::checkpoint() const {
  return next_byte_;
}
