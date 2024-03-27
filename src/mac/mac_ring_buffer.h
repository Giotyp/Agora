#include <array>

#include "logger.h"
#include "message.h"
#include "symbols.h"

template <class T>
class MacMultiRingBuffer {
 public:
  MacMultiRingBuffer<T>() {}
  void Push(T item, size_t buf_id) {
    if (IsFull(buf_id) == true) {
      AGORA_LOG_ERROR("Buffer Id %zu is full. Push failed!\n", buf_id);
      return;
    }
    size_t& tail = tail_.at(buf_id);
    r_buff_.at(buf_id).at(tail) = item;
    tail = (tail + 1) % kMacBuffSizeMax;
  }

  T Pop(size_t buf_id) {
    if (IsEmpty(buf_id) == true) {
      AGORA_LOG_ERROR("Buffer Id %zu is empty. Pop failed!\n", buf_id);
      return empty_;
    }
    size_t& head = head_.at(buf_id);
    T item = r_buff_.at(buf_id).at(head);
    r_buff_.at(buf_id).at(head) = empty_;
    head = (head + 1) % kMacBuffSizeMax;
    return item;
  }

  bool IsEmpty(size_t buf_id) {
    size_t tail = tail_.at(buf_id);
    size_t head = head_.at(buf_id);
    return (head == tail);
  }

  bool IsFull(size_t buf_id) {
    size_t tail = tail_.at(buf_id);
    size_t head = head_.at(buf_id);
    return ((tail + 1) % kMacBuffSizeMax == head);
  }

  size_t BuffSize(size_t buf_id) {
    size_t tail = tail_.at(buf_id);
    size_t head = head_.at(buf_id);
    return (tail >= head) ? (tail - head) : (kMacBuffSizeMax - head + tail);
  }

 private:
  std::array<std::array<T, kMacBuffSizeMax>, kMaxUEs> r_buff_;
  std::array<size_t, kMaxUEs> head_;
  std::array<size_t, kMaxUEs> tail_;
  T empty_;
};
