#include <algorithm>
#include <limits>

#include "context/filter_context.h"
#include "rapidjson/document.h"
#include "rapidjson/pointer.h"

namespace icehalo {

using rapidjson::Pointer;


AbstractRayPathFilter::AbstractRayPathFilter()
    : symmetry_flag_(kSymmetryNone), complementary_(false), remove_homodromous_(false) {}


bool AbstractRayPathFilter::Filter(const Crystal* crystal, RaySegment* last_r) const {
  if (remove_homodromous_ &&
      math::Dot3(last_r->dir.val(), last_r->root_ctx->first_ray_segment->dir.val()) > 1.0 - 5 * math::kFloatEps) {
    return false;
  }

  bool result = FilterPath(crystal, last_r);
  return result ^ complementary_;
}


void AbstractRayPathFilter::SetSymmetryFlag(uint8_t symmetry_flag) {
  symmetry_flag_ = symmetry_flag;
}


void AbstractRayPathFilter::AddSymmetry(Symmetry symmetry) {
  symmetry_flag_ |= symmetry;
}


uint8_t AbstractRayPathFilter::GetSymmetryFlag() const {
  return symmetry_flag_;
}


void AbstractRayPathFilter::ApplySymmetry(const Crystal* /* crystal */) {}


void AbstractRayPathFilter::EnableComplementary(bool enable) {
  complementary_ = enable;
}


bool AbstractRayPathFilter::GetComplementary() const {
  return complementary_;
}


void AbstractRayPathFilter::EnableRemoveHomodromous(bool enable) {
  remove_homodromous_ = enable;
}


bool AbstractRayPathFilter::GetRemoveHomodromous() const {
  return remove_homodromous_;
}


void AbstractRayPathFilter::SaveToJson(rapidjson::Value& root, rapidjson::Value::AllocatorType& allocator) {
  if (complementary_) {
    Pointer("/complementary").Set(root, true, allocator);
  }

  if (remove_homodromous_) {
    Pointer("/remove_homodromous").Set(root, true, allocator);
  }

  if (symmetry_flag_ != kSymmetryNone) {
    char sym_buf[16]{};
    size_t buf_idx = 0;
    if (symmetry_flag_ & kSymmetryBasal) {
      sym_buf[buf_idx++] = 'B';
    }
    if (symmetry_flag_ & kSymmetryDirection) {
      sym_buf[buf_idx++] = 'D';
    }
    if (symmetry_flag_ & kSymmetryPrism) {
      sym_buf[buf_idx] = 'P';
    }
    Pointer("/symmetry").Set(root, sym_buf, allocator);
  }
}


void AbstractRayPathFilter::LoadFromJson(const rapidjson::Value& root) {
  auto p = Pointer("/complementary").Get(root);
  if (p == nullptr) {
    EnableComplementary(false);
  } else if (p->IsBool()) {
    EnableComplementary(p->GetBool());
  } else {
    throw std::invalid_argument("<filter[%d].complementary> cannot recognize!");
  }

  p = Pointer("/remove_homodromous").Get(root);
  if (p == nullptr) {
    EnableRemoveHomodromous(false);
  } else if (p->IsBool()) {
    EnableRemoveHomodromous(p->GetBool());
  } else {
    throw std::invalid_argument("<filter[%d].remove_homodromous> cannot recognize!");
  }

  SetSymmetryFlag(kSymmetryNone);

  p = Pointer("/symmetry").Get(root);
  if (p == nullptr) {
    return;
  } else if (!p->IsString()) {
    throw std::invalid_argument("<ray_path_filter[%d].symmetry> cannot recognize!");
  }

  auto sym = p->GetString();
  for (decltype(p->GetStringLength()) i = 0; i < p->GetStringLength(); i++) {
    switch (sym[i]) {
      case 'P':
      case 'p':
        AddSymmetry(kSymmetryPrism);
        break;
      case 'B':
      case 'b':
        AddSymmetry(kSymmetryBasal);
        break;
      case 'D':
      case 'd':
        AddSymmetry(kSymmetryDirection);
        break;
      default:
        throw std::invalid_argument("<ray_path_filter[%d].symmetry> cannot recognize!");
    }
  }
}


size_t RayPathHash(const std::vector<uint16_t>& ray_path, bool reverse) {
  constexpr size_t kStep = 7;
  constexpr size_t kTotalBits = sizeof(size_t) * CHAR_BIT;

  size_t result = 0;
  size_t curr_offset = 0;
  if (reverse) {
    for (auto rit = ray_path.rbegin(); rit != ray_path.rend(); ++rit) {
      unsigned int fn = *rit;
      size_t tmp_hash = (fn << curr_offset) | (fn >> (kTotalBits - curr_offset));
      result ^= tmp_hash;
      curr_offset += kStep;
      curr_offset %= kTotalBits;
    }
  } else {
    for (unsigned int fn : ray_path) {
      size_t tmp_hash = (fn << curr_offset) | (fn >> (kTotalBits - curr_offset));
      result ^= tmp_hash;
      curr_offset += kStep;
      curr_offset %= kTotalBits;
    }
  }
  return result;
}


size_t RayPathHash(const Crystal* crystal,                  // used for get face number
                   const RaySegment* last_ray, int length,  // ray path and length
                   bool reverse) {
  constexpr size_t kStep = 7;
  constexpr size_t kTotalBits = sizeof(size_t) * CHAR_BIT;

  size_t result = 0;
  size_t curr_offset = reverse ? kStep * (length - 1) % kTotalBits : 0;
  auto p = last_ray;
  while (p->prev) {
    unsigned int fn = crystal->FaceNumber(p->face_id);
    size_t tmp_hash = (fn << curr_offset) | (fn >> (kTotalBits - curr_offset));
    result ^= tmp_hash;

    if (reverse) {
      curr_offset -= kStep;
    } else {
      curr_offset += kStep;
    }
    curr_offset %= kTotalBits;
    p = p->prev;
  }

  return result;
}


bool NoneRayPathFilter::FilterPath(const Crystal* /* crystal */, RaySegment* /* r */) const {
  return true;
}


void NoneRayPathFilter::SaveToJson(rapidjson::Value& root, rapidjson::Value::AllocatorType& allocator) {
  AbstractRayPathFilter::SaveToJson(root, allocator);

  Pointer("/type").Set(root, "none", allocator);
}


void SpecificRayPathFilter::AddPath(const std::vector<uint16_t>& path) {
  ray_paths_.emplace_back(path);
}


void SpecificRayPathFilter::ClearPaths() {
  ray_paths_.clear();
}


void SpecificRayPathFilter::ApplySymmetry(const Crystal* crystal) {
  std::vector<std::vector<uint16_t>> augmented_ray_paths;

  // Add the original path.
  for (const auto& rp : ray_paths_) {
    augmented_ray_paths.emplace_back(rp);
  }

  // Add symmetry P.
  auto period = crystal->GetFaceNumberPeriod();
  std::vector<uint16_t> tmp_ray_path;
  if (period > 0 && (symmetry_flag_ & kSymmetryPrism)) {
    std::vector<std::vector<uint16_t>> ray_paths_copy(augmented_ray_paths);
    for (const auto& rp : ray_paths_copy) {
      for (int i = 0; i < period; i++) {
        tmp_ray_path.clear();
        for (auto fn : rp) {
          if (fn != 1 && fn != 2) {
            fn = static_cast<uint16_t>((fn + period + i - 3) % period + 3);
          }
          tmp_ray_path.emplace_back(fn);
        }
        augmented_ray_paths.emplace_back(tmp_ray_path);
      }
    }
  }

  // Add symmetry B.
  if (symmetry_flag_ & kSymmetryBasal) {
    std::vector<std::vector<uint16_t>> ray_paths_copy(augmented_ray_paths);
    for (const auto& rp : ray_paths_copy) {
      tmp_ray_path.clear();
      for (auto fn : rp) {
        if (fn == 1 || fn == 2) {
          fn = static_cast<uint16_t>(fn % 2 + 1);
        }
        tmp_ray_path.emplace_back(fn);
      }
      augmented_ray_paths.emplace_back(tmp_ray_path);
    }
  }

  // Add symmetry D.
  if (period > 0 && (symmetry_flag_ & kSymmetryDirection)) {
    std::vector<std::vector<uint16_t>> ray_paths_copy(augmented_ray_paths);
    for (const auto& rp : ray_paths_copy) {
      tmp_ray_path.clear();
      for (auto fn : rp) {
        if (fn != 1 && fn != 2) {
          fn = static_cast<uint16_t>(5 + period - fn);
        }
        tmp_ray_path.emplace_back(fn);
      }
      augmented_ray_paths.emplace_back(tmp_ray_path);
    }
  }

  // Add them all.
  ray_path_hashes_.clear();
  for (const auto& rp : augmented_ray_paths) {
    ray_path_hashes_.emplace(RayPathHash(rp));
  }
}


bool SpecificRayPathFilter::FilterPath(const Crystal* crystal, RaySegment* last_r) const {
  if (ray_path_hashes_.empty()) {
    return true;
  }

  int curr_fn0 = crystal->FaceNumber(last_r->root_ctx->first_ray_segment->face_id);
  if (curr_fn0 < 0 || crystal->GetFaceNumberPeriod() < 0) {  // If do not have face number mapping.
    return true;
  }

  // First, check ray path length.
  size_t curr_ray_path_len = 0;
  auto p = last_r;
  while (p->prev) {
    int curr_fn = crystal->FaceNumber(p->face_id);
    if (curr_fn < 0) {
      return false;
    }
    p = p->prev;
    curr_ray_path_len++;
  }
  if (curr_ray_path_len == 0) {
    return false;
  }

  bool length_matched = false;
  for (const auto& rp : ray_paths_) {
    length_matched = length_matched || (curr_ray_path_len == rp.size());
  }
  if (!length_matched) {
    return false;
  }

  // Second, for each filter path, normalize current ray path, and find it in ray_path_hashes.
  auto current_ray_path_hash = RayPathHash(crystal, last_r, curr_ray_path_len, true);
  return ray_path_hashes_.count(current_ray_path_hash) != 0;
}


void SpecificRayPathFilter::SaveToJson(rapidjson::Value& root, rapidjson::Value::AllocatorType& allocator) {
  AbstractRayPathFilter::SaveToJson(root, allocator);

  Pointer("/type").Set(root, "specific", allocator);

  if (ray_paths_.size() == 1) {
    Pointer("/path/0").Create(root, allocator);
    for (const auto& f : ray_paths_[0]) {
      Pointer("/path/-").Set(root, f, allocator);
    }
  } else if (ray_paths_.size() > 1) {
    constexpr size_t kBufSize = 32;
    char buf[kBufSize]{};
    for (size_t p_idx = 0; p_idx < ray_paths_.size(); p_idx++) {
      for (size_t f_idx = 0; f_idx < ray_paths_[p_idx].size(); f_idx++) {
        std::snprintf(buf, kBufSize, "/path/%zu/%zu", p_idx, f_idx);
        Pointer(buf).Set(root, ray_paths_[p_idx][f_idx], allocator);
      }
    }
  }
}


void SpecificRayPathFilter::LoadFromJson(const rapidjson::Value& root) {
  AbstractRayPathFilter::LoadFromJson(root);

  ClearPaths();
  auto p = Pointer("/path").Get(root);
  if (p == nullptr || !p->IsArray()) {
    throw std::invalid_argument("<path> cannot recognize!");
  }
  if (!p->GetArray().Empty() && !p->GetArray()[0].IsInt() && !p->GetArray()[0].IsArray()) {
    throw std::invalid_argument("<path> cannot recognize!");
  }
  if (p->GetArray().Empty()) {
    std::fprintf(stderr, "<path> is empty. Ignore this setting.\n");
  } else if (p->GetArray()[0].IsInt()) {
    std::vector<uint16_t> tmp_path;
    for (auto const& pi : p->GetArray()) {
      if (!pi.IsInt()) {
        throw std::invalid_argument("<path> cannot recognize!");
      }
      tmp_path.emplace_back(pi.GetInt());
    }
    AddPath(tmp_path);
  } else {  // p[0].IsArray()
    for (const auto& pi : p->GetArray()) {
      if (pi.GetArray().Empty()) {
        throw std::invalid_argument("<path> cannot recognize!");
      }
      std::vector<uint16_t> tmp_path;
      for (const auto& pii : pi.GetArray()) {
        if (!pii.IsInt()) {
          throw std::invalid_argument("<path> cannot recognize!");
        }
        tmp_path.emplace_back(pii.GetInt());
      }
      AddPath(tmp_path);
    }
  }
}


void GeneralRayPathFilter::AddEntryFace(uint16_t face_number) {
  entry_faces_.emplace(face_number);
}


void GeneralRayPathFilter::AddExitFace(uint16_t face_number) {
  exit_faces_.emplace(face_number);
}


void GeneralRayPathFilter::AddHitNumber(int hit_num) {
  hit_nums_.emplace(hit_num);
}


void GeneralRayPathFilter::ClearFaces() {
  entry_faces_.clear();
  exit_faces_.clear();
}


void GeneralRayPathFilter::ClearHitNumbers() {
  hit_nums_.clear();
}


bool GeneralRayPathFilter::FilterPath(const Crystal* crystal, RaySegment* last_r) const {
  if (entry_faces_.empty() && exit_faces_.empty()) {
    return true;
  }

  if (!hit_nums_.empty()) {  // Check hit number.
    auto p = last_r;
    int n = 0;
    while (p) {
      p = p->prev;
      n++;
    }
    if (hit_nums_.count(n) == 0) {
      return false;
    }
  }

  int curr_entry_fn = crystal->FaceNumber(last_r->root_ctx->first_ray_segment->face_id);
  int curr_exit_fn = crystal->FaceNumber(last_r->face_id);
  if (curr_entry_fn < 0 || curr_exit_fn < 0 ||
      crystal->GetFaceNumberPeriod() < 0) {  // If do not have a face number mapping
    return true;
  }

  return entry_faces_.count(static_cast<uint16_t>(curr_entry_fn)) != 0 &&
         exit_faces_.count(static_cast<uint16_t>(curr_exit_fn)) != 0;
}


void GeneralRayPathFilter::SaveToJson(rapidjson::Value& root, rapidjson::Value::AllocatorType& allocator) {
  AbstractRayPathFilter::SaveToJson(root, allocator);

  Pointer("/type").Set(root, "general", allocator);

  if (!entry_faces_.empty()) {
    Pointer("/entry/0").Create(root, allocator);
    for (const auto& f : entry_faces_) {
      Pointer("/entry/-").Set(root, f, allocator);
    }
  }

  if (!exit_faces_.empty()) {
    Pointer("/exit/0").Create(root, allocator);
    for (const auto& f : exit_faces_) {
      Pointer("/exit/-").Set(root, f, allocator);
    }
  }

  if (!hit_nums_.empty()) {
    Pointer("/hit/0").Create(root, allocator);
    for (const auto& n : hit_nums_) {
      Pointer("/hit/-").Set(root, n, allocator);
    }
  }
}


void GeneralRayPathFilter::LoadFromJson(const rapidjson::Value& root) {
  AbstractRayPathFilter::LoadFromJson(root);

  ClearHitNumbers();
  ClearFaces();

  auto p = Pointer("/entry").Get(root);
  if (p == nullptr || !p->IsArray()) {
    throw std::invalid_argument("<entry> cannot recognize!");
  }
  for (const auto& pi : p->GetArray()) {
    if (!pi.IsInt()) {
      throw std::invalid_argument("<entry> cannot recognize!");
    }
    AddEntryFace(pi.GetInt());
  }

  p = Pointer("/exit").Get(root);
  if (p == nullptr || !p->IsArray()) {
    throw std::invalid_argument("<exit> cannot recognize!");
  }
  for (const auto& pi : p->GetArray()) {
    if (!pi.IsInt()) {
      throw std::invalid_argument("<exit> cannot recognize!");
    }
    AddExitFace(pi.GetInt());
  }

  p = Pointer("/hit").Get(root);
  if (p == nullptr) {
    std::fprintf(stderr, "<hit> is empty. Ignore this setting.\n");
  } else if (!p->IsArray()) {
    throw std::invalid_argument("<hit> cannot recognize!");
  } else {
    for (const auto& pi : p->GetArray()) {
      if (!pi.IsInt()) {
        throw std::invalid_argument("<hit> cannot recognize!");
      }
      AddHitNumber(pi.GetInt());
    }
  }
}


}  // namespace icehalo
