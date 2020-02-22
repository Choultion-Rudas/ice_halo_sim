#include "context/render_context.hpp"

#include <algorithm>

#include "core/render.hpp"
#include "rapidjson/filereadstream.h"
#include "rapidjson/pointer.h"
#include "util/log.hpp"


namespace icehalo {

using rapidjson::Pointer;

RenderSplitter::RenderSplitter() : type(RenderSplitterType::kNone), top_halo_num(0), crystal_filters{} {}


RenderContext::RenderContext()
    : ray_color_{ 1.0f, 1.0f, 1.0f }, background_color_{ 0.0f, 0.0f, 0.0f }, intensity_(1.0f), image_width_(0),
      image_height_(0), offset_x_(0), offset_y_(0), visible_range_(VisibleRange::kUpper),
      color_compact_level_(ColorCompactLevel::kTrueColor), splitter_{} {}


RenderContextPtrU RenderContext::CreateDefault() {
  RenderContextPtrU render_ctx{ new RenderContext };
  return render_ctx;
}


constexpr float RenderContext::kMinIntensity;
constexpr float RenderContext::kMaxIntensity;
constexpr int RenderContext::kMaxImageSize;
constexpr int RenderContext::kMaxTopHaloNumber;


const float* RenderContext::GetRayColor() const {
  return ray_color_;
}


void RenderContext::SetRayColor(float r, float g, float b) {
  ray_color_[0] = std::min(std::max(r, 0.0f), 1.0f);
  ray_color_[1] = std::min(std::max(g, 0.0f), 1.0f);
  ray_color_[2] = std::min(std::max(b, 0.0f), 1.0f);
}


void RenderContext::ResetRayColor() {
  UseRealRayColor();
}


void RenderContext::UseRealRayColor() {
  ray_color_[0] = -1.0f;
  ray_color_[1] = -1.0f;
  ray_color_[2] = -1.0f;
}


const float* RenderContext::GetBackgroundColor() const {
  return background_color_;
}


void RenderContext::SetBackgroundColor(float r, float g, float b) {
  background_color_[0] = std::min(std::max(r, 0.0f), 1.0f);
  background_color_[1] = std::min(std::max(g, 0.0f), 1.0f);
  background_color_[2] = std::min(std::max(b, 0.0f), 1.0f);
}


void RenderContext::ResetBackgroundColor() {
  background_color_[0] = 0.0f;
  background_color_[1] = 0.0f;
  background_color_[2] = 0.0f;
}


void RenderContext::UseSkyBackground() {
  background_color_[0] = -1.0f;
  background_color_[1] = -1.0f;
  background_color_[2] = -1.0f;
}


float RenderContext::GetIntensity() const {
  return intensity_;
}


void RenderContext::SetIntensity(float intensity) {
  intensity_ = std::min(std::max(intensity, kMinIntensity), kMaxIntensity);
}


int RenderContext::GetImageWidth() const {
  return image_width_;
}


int RenderContext::GetImageHeight() const {
  return image_height_;
}


void RenderContext::SetImageWidth(int w) {
  image_width_ = std::min(std::max(w, 0), kMaxImageSize);
}


void RenderContext::SetImageHeight(int h) {
  image_height_ = std::min(std::max(h, 0), kMaxImageSize);
}


int RenderContext::GetImageOffsetX() const {
  return offset_x_;
}


int RenderContext::GetImageOffsetY() const {
  return offset_y_;
}


void RenderContext::SetImageOffsetX(int offset_x) {
  offset_x_ = offset_x;
}


void RenderContext::SetImageOffsetY(int offset_y) {
  offset_y_ = offset_y;
}


const RenderSplitter& RenderContext::GetSplitter() const {
  return splitter_;
}


int RenderContext::GetSplitNumber() const {
  switch (splitter_.type) {
    case RenderSplitterType::kNone:
      return 0;
    case RenderSplitterType::kTopHalo:
      return splitter_.top_halo_num;
    case RenderSplitterType::kFilter:
      return static_cast<int>(splitter_.crystal_filters.size());
  }
}


void RenderContext::SetColorCompactLevel(ColorCompactLevel level) {
  color_compact_level_ = level;
}


ColorCompactLevel RenderContext::GetColorCompactLevel() const {
  return color_compact_level_;
}


VisibleRange RenderContext::GetVisibleRange() const {
  return visible_range_;
}


void RenderContext::SetVisibleRange(VisibleRange r) {
  visible_range_ = r;
}


void RenderContext::SaveToJson(rapidjson::Value& root, rapidjson::Value::AllocatorType& allocator) {
  root.Clear();

  Pointer("/width").Set(root, image_width_, allocator);
  Pointer("/height").Set(root, image_height_, allocator);
  switch (visible_range_) {
    case VisibleRange::kLower:
      Pointer("/visible_semi_sphere").Set(root, "lower", allocator);
      break;
    case VisibleRange::kFront:
      Pointer("/visible_semi_sphere").Set(root, "camera", allocator);
      break;
    case VisibleRange::kFull:
      Pointer("/visible_semi_sphere").Set(root, "full", allocator);
      break;
    case VisibleRange::kUpper:
    default:
      Pointer("/visible_semi_sphere").Set(root, "upper", allocator);
      break;
  }
  Pointer("/intensity_factor").Set(root, intensity_, allocator);

  Pointer("/offset/0").Set(root, offset_x_, allocator);
  Pointer("/offset/-").Set(root, offset_y_, allocator);

  switch (splitter_.type) {
    case RenderSplitterType::kTopHalo:
      Pointer("/splitter/type").Set(root, "top_halo", allocator);
      Pointer("/splitter/param").Set(root, splitter_.top_halo_num, allocator);
      break;
    case RenderSplitterType::kFilter: {
      char id_buf[128];
      Pointer("/splitter/type").Set(root, "filter", allocator);
      for (size_t i = 0; i < splitter_.crystal_filters.size(); i++) {
        const auto& ids = splitter_.crystal_filters[i];
        for (size_t j = 0; j < ids.size(); j++) {
          std::snprintf(id_buf, 128, "/splitter/%zu/%zu", i, j);
          Pointer(id_buf).Set(root, ids[j], allocator);
        }
      }
    } break;
    case RenderSplitterType::kNone:
      break;
  }

  switch (color_compact_level_) {
    case ColorCompactLevel::kTrueColor:
      Pointer("/ray_compact_level").Set(root, "true_color", allocator);
      break;
    case ColorCompactLevel::kMonochrome:
      Pointer("/ray_compact_level").Set(root, "monochrome", allocator);
      break;
    case ColorCompactLevel::kLowQuality:
      Pointer("/ray_compact_level").Set(root, "low_quality", allocator);
      break;
  }

  Pointer("/background_color/0").Set(root, background_color_[0], allocator);
  Pointer("/background_color/-").Set(root, background_color_[1], allocator);
  Pointer("/background_color/-").Set(root, background_color_[2], allocator);

  if (ray_color_[0] < 0) {
    Pointer("/ray_color").Set(root, "real", allocator);
  } else {
    Pointer("/ray_color/0").Set(root, ray_color_[0], allocator);
    Pointer("/ray_color/-").Set(root, ray_color_[1], allocator);
    Pointer("/ray_color/-").Set(root, ray_color_[2], allocator);
  }
}


void RenderContext::LoadFromJson(const rapidjson::Value& root) {
  SetImageWidth(800);
  auto p = Pointer("/width").Get(root);
  if (p == nullptr) {
    LOG_INFO("Render config missing <width>, using default 800!");
  } else if (!p->IsInt()) {
    LOG_INFO("Render config <width> is not an integer, using default 800!");
  } else {
    SetImageWidth(p->GetInt());
  }

  SetImageHeight(800);
  p = Pointer("/height").Get(root);
  if (p == nullptr) {
    LOG_INFO("Render config missing <height>, using default 800!");
  } else if (!p->IsInt()) {
    LOG_INFO("Render config <height> is not an integer, using default 800!");
  } else {
    SetImageHeight(p->GetInt());
  }

  SetVisibleRange(VisibleRange::kUpper);
  p = Pointer("/visible_semi_sphere").Get(root);
  if (p == nullptr) {
    LOG_INFO("Render config missing <visible_semi_sphere>, using default kUpper!");
  } else if (!p->IsString()) {
    LOG_INFO("Render config <visible_semi_sphere> is not a string, using default kUpper!");
  } else if (*p == "upper") {
    SetVisibleRange(VisibleRange::kUpper);
  } else if (*p == "lower") {
    SetVisibleRange(VisibleRange::kLower);
  } else if (*p == "camera") {
    SetVisibleRange(VisibleRange::kFront);
  } else if (*p == "full") {
    SetVisibleRange(VisibleRange::kFull);
  } else {
    std::fprintf(stderr,
                 "\nWARNING! Render config <visible_semi_sphere> cannot be recognized, using default kUpper!\n");
  }

  SetIntensity(1.0f);
  p = Pointer("/intensity_factor").Get(root);
  if (p == nullptr) {
    LOG_INFO("Render config missing <intensity_factor>, using default 1.0!");
  } else if (!p->IsNumber()) {
    LOG_INFO("Render config <intensity_factor> is not a number, using default 1.0!");
  } else {
    auto f = static_cast<float>(p->GetDouble());
    f = std::max(std::min(f, RenderContext::kMaxIntensity), RenderContext::kMinIntensity);
    SetIntensity(f);
  }

  SetImageOffsetX(0);
  SetImageOffsetY(0);
  p = Pointer("/offset").Get(root);
  if (p == nullptr) {
    LOG_INFO("Render config missing <offset>, using default [0, 0]!");
  } else if (!p->IsArray()) {
    LOG_INFO("Render config <offset> is not an array, using default [0, 0]!");
  } else if (p->Size() != 2 || !(*p)[0].IsInt() || !(*p)[1].IsInt()) {
    LOG_INFO("Config <render.offset> cannot be recognized, using default [0, 0]!");
  } else {
    int offset_x = (*p)[0].GetInt();
    int offset_y = (*p)[1].GetInt();
    offset_x = std::max(std::min(offset_x, static_cast<int>(RenderContext::kMaxImageSize / 2)),
                        -static_cast<int>(RenderContext::kMaxImageSize / 2));
    offset_y = std::max(std::min(offset_y, static_cast<int>(RenderContext::kMaxImageSize / 2)),
                        -static_cast<int>(RenderContext::kMaxImageSize / 2));
    SetImageOffsetX(offset_x);
    SetImageOffsetY(offset_y);
  }

  SetColorCompactLevel(ColorCompactLevel::kTrueColor);
  p = Pointer("/color_compact_level").Get(root);
  if (p == nullptr) {
    LOG_INFO("Render config missing <color_compact_level>, use default true color!");
  } else if (!p->IsString()) {
    LOG_INFO("Render config <color_compact_level> is not a string, use default true color!");
  } else {
    if (*p == "true_color") {
      color_compact_level_ = ColorCompactLevel::kTrueColor;
    } else if (*p == "monochrome") {
      color_compact_level_ = ColorCompactLevel::kMonochrome;
    } else if (*p == "low_quality") {
      color_compact_level_ = ColorCompactLevel::kLowQuality;
    } else {
      std::fprintf(stderr,
                   "\nWARNING! Render config <color_compact_level> cannot recognize, use default true color!\n");
    }
  }

  splitter_ = {};
  p = Pointer("/splitter").Get(root);
  if (p == nullptr) {
    LOG_INFO("Render config missing <splitter>, use default!");
  } else if (!p->IsObject()) {
    LOG_INFO("Render config <splitter> is not an object, use default!");
  } else {
    auto pp = Pointer("/type").Get(*p);
    if (pp && pp->IsString() && *pp == "top_halo") {
      splitter_.type = RenderSplitterType::kTopHalo;
    } else if (pp && pp->IsString() && *pp == "filter") {
      splitter_.type = RenderSplitterType::kFilter;
    }

    pp = Pointer("/param").Get(*p);
    if (pp && pp->IsInt() && splitter_.type == RenderSplitterType::kTopHalo) {
      splitter_.top_halo_num = pp->GetInt();
    } else if (pp && pp->IsArray() && splitter_.type == RenderSplitterType::kFilter) {
      std::vector<std::vector<ShortIdType>> crystal_filters;
      for (const auto& ppi : pp->GetArray()) {
        if (!ppi.IsArray() || ppi.GetArray().Size() % 2 != 0) {
          LOG_INFO("crystal filter param not recognize!");
          continue;
        }
        std::vector<ShortIdType> tmp_ids{};
        for (const auto& fid : ppi.GetArray()) {
          if (!fid.IsUint()) {
            LOG_INFO("crystal filter param not recognize!");
            continue;
          }
          tmp_ids.emplace_back(fid.GetUint());
        }
        if (!tmp_ids.empty()) {
          crystal_filters.emplace_back(tmp_ids);
        }
      }
      if (!crystal_filters.empty()) {
        splitter_.crystal_filters = crystal_filters;
      }
    }
  }

  ResetBackgroundColor();
  p = Pointer("/background_color").Get(root);
  if (p == nullptr) {
    LOG_INFO("Config missing <render.background_color>, using default [0,0,0]!");
  } else if (!p->IsArray()) {
    LOG_INFO("Config <render.background_color> is not an array, using default [0,0,0]!");
  } else if (p->Size() != 3 || !(*p)[0].IsNumber()) {
    LOG_INFO("Config <render.background_color> cannot be recognized, using default [0,0,0]!");
  } else {
    auto pa = p->GetArray();
    float r = static_cast<float>(std::min(std::max(pa[0].GetDouble(), 0.0), 1.0));
    float g = static_cast<float>(std::min(std::max(pa[0].GetDouble(), 0.0), 1.0));
    float b = static_cast<float>(std::min(std::max(pa[0].GetDouble(), 0.0), 1.0));
    SetBackgroundColor(r, g, b);
  }

  ResetRayColor();
  p = Pointer("/ray_color").Get(root);
  if (p == nullptr) {
    LOG_INFO("Config missing <render.ray_color>, using default real color!");
  } else if (!p->IsArray() && !p->IsString()) {
    LOG_INFO("Config <render.ray_color> is not an array nor a string, ");
    LOG_INFO("using default real color!");
  } else if (p->IsArray() && (p->Size() != 3 || !(*p)[0].IsNumber())) {
    LOG_INFO("Config <render.ray_color> cannot be recognized, using default real color!");
  } else if (p->IsString() && (*p) != "real") {
    LOG_INFO("Config <render.ray_color> cannot be recognized, using default real color!");
  } else if (p->IsArray()) {
    auto pa = p->GetArray();
    float r = static_cast<float>(std::min(std::max(pa[0].GetDouble(), 0.0), 1.0));
    float g = static_cast<float>(std::min(std::max(pa[1].GetDouble(), 0.0), 1.0));
    float b = static_cast<float>(std::min(std::max(pa[2].GetDouble(), 0.0), 1.0));
    SetRayColor(r, g, b);
  }
}

}  // namespace icehalo
