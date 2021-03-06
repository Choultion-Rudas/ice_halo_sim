#include "util/obj_pool.h"

#include "core/optics.h"

namespace icehalo {

constexpr uint32_t kInvalidIndex = 0xffffffff;


template <typename T>
ObjectPool<T>::~ObjectPool() {
  for (auto seg : objects_) {
    delete[] seg;
  }
  objects_.clear();
}


template <typename T>
void ObjectPool<T>::Clear() {
  next_unused_id_ = 0;
  current_chunk_id_ = 0;
  deserialized_chunk_size_ = 0;
}


template <typename T>
T* ObjectPool<T>::GetPointerFromSerializeData(T* dummy_ptr) {
  auto combined_id = reinterpret_cast<uintptr_t>(dummy_ptr);
  uint32_t chunk_id = (combined_id & 0xffffffff00000000) >> 32;
  uint32_t obj_id = (combined_id & 0x00000000ffffffff);

  if (deserialized_chunk_size_ == 0) {
    return nullptr;
  }
  if (chunk_id == kInvalidIndex || obj_id == kInvalidIndex) {
    return nullptr;
  }

  size_t id = chunk_id * deserialized_chunk_size_ + obj_id;
  uint32_t this_chunk_id = id / kChunkSize;
  uint32_t this_obj_id = id % kChunkSize;
  if (this_chunk_id >= objects_.size()) {
    return nullptr;
  }

  return objects_[this_chunk_id] + this_obj_id;
}


template <typename T>
T* ObjectPool<T>::GetPointerFromSerializeData(uint32_t chunk_id, uint32_t obj_id) {
  return GetPointerFromSerializeData(reinterpret_cast<T*>(CombineU32AsPointer(chunk_id, obj_id)));
}


template <typename T>
std::tuple<uint32_t, uint32_t> ObjectPool<T>::GetObjectSerializeIndex(T* obj) {
  if (!obj) {
    return { kInvalidIndex, kInvalidIndex };
  }

  uint32_t chunk_id = 0;
  T* last_chunk = nullptr;
  for (const auto& chunk : objects_) {
    if (last_chunk && obj < chunk) {
      return { chunk_id - 1, static_cast<uint32_t>(obj - last_chunk) };
    }
    chunk_id++;
    last_chunk = chunk;
  }
  if (last_chunk && obj < objects_.back() + kChunkSize) {
    return { chunk_id - 1, static_cast<uint32_t>(obj - last_chunk) };
  } else {
    return { kInvalidIndex, kInvalidIndex };
  }
}


template <typename T>
void ObjectPool<T>::Map(std::function<void(T&)> f) {
  const std::lock_guard<std::mutex> lock(id_mutex_);
  for (const auto& chunk : objects_) {
    size_t chunk_size = (chunk == objects_.back() ? next_unused_id_.load() : kChunkSize);
    for (size_t j = 0; j < chunk_size; j++) {
      f(chunk[j]);
    }
  }
}


template <typename T>
ObjectPool<T>* ObjectPool<T>::GetInstance() {
  static auto instance = new ObjectPool<T>();
  return instance;
}


template <typename T>
ObjectPool<T>::ObjectPool() : current_chunk_id_(0), next_unused_id_(0), deserialized_chunk_size_(0) {
  auto* pool = new T[kChunkSize];
  objects_.emplace_back(pool);
}


template <typename T>
uint32_t ObjectPool<T>::RefreshChunkIndex() {
  auto id = next_unused_id_.fetch_add(1);
  if (id >= kChunkSize) {
    const std::lock_guard<std::mutex> lock(id_mutex_);
    id = next_unused_id_;
    if (id > kChunkSize) {
      auto seg_size = objects_.size();
      if (current_chunk_id_ + 1 >= seg_size) {
        auto* curr_pool = new T[kChunkSize];
        objects_.emplace_back(curr_pool);
        current_chunk_id_ = seg_size;
      } else {
        current_chunk_id_++;
      }
      id = 0;
      next_unused_id_ = 0;
    }
  }
  return id;
}


template <typename T>
void ObjectPool<T>::Serialize(File& file, bool with_boi) const {
  if (with_boi) {
    file.Write(ISerializable::kDefaultBoi);
  }

  size_t total_num = kChunkSize * (objects_.size() - 1) + next_unused_id_;
  file.Write(total_num);
  file.Write(kChunkSize);

  for (const auto& chunk : objects_) {
    size_t num = (chunk == objects_.back() ? next_unused_id_.load() : kChunkSize);
    for (size_t i = 0; i < num; i++) {
      chunk[i].Serialize(file, false);
    }
  }
}


template <typename T>
void ObjectPool<T>::Deserialize(File& file, endian::Endianness endianness) {
  const std::lock_guard<std::mutex> lock(id_mutex_);

  endianness = CheckEndianness(file, endianness);
  bool need_swap = (endianness != endian::kCompileEndian);

  size_t total_num;
  file.Read(&total_num);
  if (need_swap) {
    endian::ByteSwap::Swap(&total_num);
  }

  size_t chunk_size;
  file.Read(&chunk_size);
  if (need_swap) {
    endian::ByteSwap::Swap(&chunk_size);
  }
  if (chunk_size == 0) {
    throw std::invalid_argument("Chunk size is invalid!");
  }

  Clear();
  deserialized_chunk_size_ = chunk_size;
  size_t chunks = total_num / kChunkSize + (total_num % kChunkSize ? 1 : 0);
  for (current_chunk_id_ = 0; current_chunk_id_ < chunks; current_chunk_id_++) {
    if (current_chunk_id_ >= objects_.size()) {
      objects_.emplace_back(new T[kChunkSize]);
    }
    auto* chunk = objects_[current_chunk_id_];
    size_t curr_num = kChunkSize;
    if (current_chunk_id_ + 1 == chunks) {
      curr_num = total_num % kChunkSize;
    }
    for (next_unused_id_ = 0; next_unused_id_ < curr_num; next_unused_id_++) {
      chunk[next_unused_id_].Deserialize(file, endianness);
    }
  }
}

template class ObjectPool<RaySegment>;
template class ObjectPool<RayInfo>;

}  // namespace icehalo