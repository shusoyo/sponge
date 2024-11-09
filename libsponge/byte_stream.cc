#include "byte_stream.hh"

#include <cstddef>
#include <string>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in
// `byte_stream.hh`

using std::string;

ByteStream::ByteStream(const size_t capacity) : capacity_(capacity) {}

size_t ByteStream::write(const string &data) {
  size_t can_write_size = std::min(remaining_capacity(), data.size());
  bytes_pushed_ += can_write_size;

  if (can_write_size > 0) {
    buffer.emplace_back(data.substr(0, can_write_size));
  }

  return can_write_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
  string output {};
  size_t length = len;

  for (auto x : buffer) {
    if (length >= x.size()) {
      output.append(x);
      length -= x.size();
    } else {
      output.append(x.substr(0, length));
      break;
    }
  }

  return output;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
  size_t length = len;

  while (length != 0) {
    auto &s = buffer.front();
    if (length >= s.size()) {
      length -= s.size();
      buffer.pop_front();
    } else {
      s = s.substr(length);
      break;
    }
  }

  bytes_popped_ += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
string ByteStream::read(const size_t len) {
  string s = peek_output(len);
  pop_output(len);
  return s;
}

void ByteStream::end_input() {
  input_ended_ = true;
}

bool ByteStream::input_ended() const {
  return input_ended_;
}

size_t ByteStream::buffer_size() const {
  return bytes_pushed_ - bytes_popped_;
}

bool ByteStream::buffer_empty() const {
  return buffer_size() == 0;
}

bool ByteStream::eof() const {
  return (bytes_popped_ == bytes_pushed_) and input_ended();
}

size_t ByteStream::bytes_written() const {
  return bytes_pushed_;
}

size_t ByteStream::bytes_read() const {
  return bytes_popped_;
}

size_t ByteStream::remaining_capacity() const {
  return capacity_ - buffer_size();
}
