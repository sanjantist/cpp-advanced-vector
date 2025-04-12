#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

template <typename T>
class RawMemory {
 public:
  RawMemory() = default;
  RawMemory(const RawMemory&) = delete;
  RawMemory& operator=(const RawMemory& rhs);

  RawMemory(RawMemory&& other) noexcept
      : buffer_(other.buffer_), capacity_(other.capacity_) {
    other.buffer_ = nullptr;
    other.capacity_ = 0;
  }

  RawMemory& operator=(RawMemory&& other) noexcept {
    if (this != &other) {
      Swap(other);
    }
    return *this;
  }

  explicit RawMemory(size_t capacity)
      : buffer_(Allocate(capacity)), capacity_(capacity) {}

  ~RawMemory() { Deallocate(buffer_); }

  T* operator+(size_t offset) noexcept {
    assert(offset <= capacity_);
    return buffer_ + offset;
  }

  const T* operator+(size_t offset) const noexcept {
    return const_cast<RawMemory&>(*this) + offset;
  }

  const T& operator[](size_t index) const noexcept {
    return const_cast<RawMemory&>(*this)[index];
  }

  T& operator[](size_t index) noexcept {
    assert(index < capacity_);
    return buffer_[index];
  }

  void Swap(RawMemory& other) noexcept {
    std::swap(buffer_, other.buffer_);
    std::swap(capacity_, other.capacity_);
  }

  const T* GetAddress() const noexcept { return buffer_; }

  T* GetAddress() noexcept { return buffer_; }

  size_t Capacity() const { return capacity_; }

 private:
  static T* Allocate(size_t n) {
    return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
  }

  static void Deallocate(T* buf) noexcept { operator delete(buf); }

  T* buffer_ = nullptr;
  size_t capacity_ = 0;
};

template <typename T>
class Vector {
 public:
  using iterator = T*;
  using const_iterator = const T*;

  Vector() noexcept = default;
  Vector(size_t size);
  Vector(const Vector& other);
  Vector(Vector&& other) noexcept;
  ~Vector();

  iterator begin() noexcept;
  iterator end() noexcept;
  const_iterator begin() const noexcept;
  const_iterator end() const noexcept;
  const_iterator cbegin() const noexcept;
  const_iterator cend() const noexcept;

  template <typename... Args>
  iterator Emplace(const_iterator pos, Args&&... args);
  iterator Erase(const_iterator pos);
  iterator Insert(const_iterator pos, const T& value);
  iterator Insert(const_iterator pos, T&& value);

  size_t Size() const noexcept;
  size_t Capacity() const noexcept;

  const T& operator[](size_t index) const noexcept;
  T& operator[](size_t index) noexcept;

  Vector& operator=(const Vector& rhs);
  Vector& operator=(Vector&& rhs) noexcept;

  void Resize(size_t new_size);
  void PushBack(const T& value);
  void PushBack(T&& value);
  template <typename... Args>
  T& EmplaceBack(Args&&... args);
  void PopBack() noexcept;
  void Reserve(size_t new_capacity);

  void Swap(Vector& other) noexcept;

 private:
  RawMemory<T> data_;
  size_t size_ = 0;

  template <typename U>
  void PushBackImpl(U&& value);
};

//----------implementations----------

template <typename T>
void Vector<T>::PushBack(const T& value) {
  PushBackImpl(value);
}

template <typename T>
void Vector<T>::PushBack(T&& value) {
  PushBackImpl(std::move(value));
}

template <typename T>
void Vector<T>::Reserve(size_t new_capacity) {
  if (new_capacity <= data_.Capacity()) {
    return;
  }

  RawMemory<T> new_data(new_capacity);
  if constexpr (std::is_nothrow_move_constructible_v<T> ||
                !std::is_copy_constructible_v<T>) {
    std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
  } else {
    std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
  }
  std::destroy_n(data_.GetAddress(), size_);
  data_.Swap(new_data);
}

template <typename T>
void Vector<T>::Swap(Vector& other) noexcept {
  other.data_.Swap(data_);
  std::swap(size_, other.size_);
}

template <typename T>
void Vector<T>::Resize(size_t new_size) {
  Reserve(new_size);
  if (new_size < size_) {
    std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
  } else {
    std::uninitialized_value_construct_n(data_.GetAddress() + size_,
                                         new_size - size_);
  }
  size_ = new_size;
}

template <typename T>
Vector<T>& Vector<T>::operator=(Vector&& rhs) noexcept {
  if (this != &rhs) {
    Swap(rhs);
  }
  return *this;
}

template <typename T>
Vector<T>& Vector<T>::operator=(const Vector& rhs) {
  if (this != &rhs) {
    if (rhs.size_ > data_.Capacity()) {
      Vector<T> tmp(rhs);
      Swap(tmp);
    } else {
      if (rhs.size_ <= size_) {
        std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.size_,
                  data_.GetAddress());
        std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
      } else {
        std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_,
                  data_.GetAddress());
        std::uninitialized_copy_n(rhs.data_.GetAddress() + size_,
                                  rhs.size_ - size_,
                                  data_.GetAddress() + size_);
      }
      size_ = rhs.size_;
    }
  }
  return *this;
}

template <typename T>
T& Vector<T>::operator[](size_t index) noexcept {
  assert(index < size_);
  return data_[index];
}

template <typename T>
const T& Vector<T>::operator[](size_t index) const noexcept {
  return const_cast<Vector&>(*this)[index];
}

template <typename T>
size_t Vector<T>::Capacity() const noexcept {
  return data_.Capacity();
}

template <typename T>
size_t Vector<T>::Size() const noexcept {
  return size_;
}

template <typename T>
Vector<T>::~Vector() {
  std::destroy_n(data_.GetAddress(), size_);
}

template <typename T>
Vector<T>::Vector(Vector&& other) noexcept : data_(), size_(0) {
  Swap(other);
}

template <typename T>
Vector<T>::Vector(const Vector& other)
    : data_(RawMemory<T>(other.size_)), size_(other.size_) {
  std::uninitialized_copy_n(other.data_.GetAddress(), other.size_,
                            data_.GetAddress());
}

template <typename T>
Vector<T>::Vector(size_t size) : data_(RawMemory<T>(size)), size_(size) {
  std::uninitialized_value_construct_n(data_.GetAddress(), size);
}

template <typename T>
void Vector<T>::PopBack() noexcept {
  std::destroy_at(end() - 1);
  --size_;
}

template <typename T>
template <typename U>
void Vector<T>::PushBackImpl(U&& value) {
  if (size_ == Capacity()) {
    RawMemory<T> temp(size_ == 0 ? 1 : size_ * 2);
    new (temp.GetAddress() + size_) T(std::forward<U>(value));
    if constexpr (std::is_nothrow_move_constructible_v<T> ||
                  !std::is_copy_constructible_v<T>) {
      std::uninitialized_move_n(data_.GetAddress(), size_, temp.GetAddress());
    } else {
      std::uninitialized_copy_n(data_.GetAddress(), size_, temp.GetAddress());
    }
    std::destroy_n(data_.GetAddress(), size_);
    data_.Swap(temp);
  } else {
    new (data_.GetAddress() + size_) T(std::forward<U>(value));
  }
  ++size_;
}

template <typename T>
template <typename... Args>
T& Vector<T>::EmplaceBack(Args&&... args) {
  if (size_ == Capacity()) {
    RawMemory<T> temp(size_ == 0 ? 1 : size_ * 2);
    new (temp.GetAddress() + size_) T(std::forward<Args>(args)...);
    if constexpr (std::is_nothrow_move_constructible_v<T> ||
                  !std::is_copy_constructible_v<T>) {
      std::uninitialized_move_n(data_.GetAddress(), size_, temp.GetAddress());
    } else {
      std::uninitialized_copy_n(data_.GetAddress(), size_, temp.GetAddress());
    }
    std::destroy_n(data_.GetAddress(), size_);
    data_.Swap(temp);
  } else {
    new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
  }
  ++size_;
  return *(data_.GetAddress() + size_ - 1);
}

template <typename T>
Vector<T>::iterator Vector<T>::begin() noexcept {
  return data_.GetAddress();
}

template <typename T>
Vector<T>::iterator Vector<T>::end() noexcept {
  return data_.GetAddress() + size_;
}

template <typename T>
Vector<T>::const_iterator Vector<T>::begin() const noexcept {
  return data_.GetAddress();
}

template <typename T>
Vector<T>::const_iterator Vector<T>::end() const noexcept {
  return data_.GetAddress() + size_;
}

template <typename T>
Vector<T>::const_iterator Vector<T>::cbegin() const noexcept {
  return data_.GetAddress();
}

template <typename T>
Vector<T>::const_iterator Vector<T>::cend() const noexcept {
  return data_.GetAddress() + size_;
}

template <typename T>
template <typename... Args>
typename Vector<T>::iterator Vector<T>::Emplace(const_iterator pos,
                                                Args&&... args) {
  auto pos_it = const_cast<iterator>(pos);
  size_t pos_index = pos_it - begin();

  if (size_ < Capacity()) {
    if (pos_it == end()) {
      new (end()) T(std::forward<Args>(args)...);
      ++size_;
      return end() - 1;
    } else {
      T tmp(std::forward<Args>(args)...);
      if constexpr (std::is_nothrow_move_constructible_v<T> ||
                    !std::is_copy_constructible_v<T>) {
        std::uninitialized_move(end() - 1, end(), end());
        std::move_backward(pos_it, end() - 1, end());
        data_[pos_index] = std::move(tmp);
      } else {
        new (end()) T(*(end() - 1));
        std::uninitialized_copy(end() - 1, end(), end());
        std::copy_backward(pos_it, end() - 1, end());
        data_[pos_index] = tmp;
      }
      ++size_;
      return begin() + pos_index;
    }
  } else {
    RawMemory<T> temp(size_ == 0 ? 1 : size_ * 2);
    T* new_ptr = temp.GetAddress();
    new (new_ptr + pos_index) T(std::forward<Args>(args)...);

    if (pos_it != begin()) {
      if constexpr (std::is_nothrow_move_constructible_v<T> ||
                    !std::is_copy_constructible_v<T>)
        std::uninitialized_move(begin(), pos_it, new_ptr);
      else
        std::uninitialized_copy(begin(), pos_it, new_ptr);
    }

    if (pos_it != end()) {
      if constexpr (std::is_nothrow_move_constructible_v<T> ||
                    !std::is_copy_constructible_v<T>)
        std::uninitialized_move(pos_it, end(), new_ptr + pos_index + 1);
      else
        std::uninitialized_copy(pos_it, end(), new_ptr + pos_index + 1);
    }
    std::destroy_n(begin(), size_);

    data_.Swap(temp);
    ++size_;
    return begin() + pos_index;
  }
}

template <typename T>
Vector<T>::iterator Vector<T>::Erase(Vector<T>::const_iterator pos) {
  auto pos_it = const_cast<iterator>(pos);
  size_t pos_index = pos_it - begin();
  if constexpr (std::is_nothrow_move_constructible_v<T> ||
                !std::is_copy_constructible_v<T>) {
    std::move(pos_it + 1, end(), pos_it);
  } else {
    std::copy(pos_it + 1, end(), pos_it);
  }
  std::destroy_at(end() - 1);
  --size_;
  return begin() + pos_index;
}

template <typename T>
Vector<T>::iterator Vector<T>::Insert(Vector<T>::const_iterator pos,
                                      const T& value) {
  return Emplace(pos, value);
}

template <typename T>
Vector<T>::iterator Vector<T>::Insert(Vector<T>::const_iterator pos,
                                      T&& value) {
  return Emplace(pos, std::move(value));
}