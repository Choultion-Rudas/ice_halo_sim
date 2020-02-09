#ifndef SRC_CORE_SIMULATION_H_
#define SRC_CORE_SIMULATION_H_

#include <vector>

#include "context/context.h"
#include "core/crystal.h"
#include "core/optics.h"
#include "io/serialize.h"

namespace icehalo {

struct SimpleRayData : public ISerializable {
  explicit SimpleRayData(size_t num);

  float wavelength;
  float weight;
  std::unique_ptr<float[]> buf;
  size_t size;

  /**
   * @brief Serialize self to a file.
   *
   * This class is a simple wrapper for a piece of contiguous memory. So this method just simply write
   * all float numbers to a file.
   *
   * The file layout is:
   * float,                 // wavelength
   * float,                 // weight
   * uint64,                // number N
   * (float * 4) * N,       // x, y, z, w
   *
   * @param file
   * @param with_boi
   */
  void Serialize(File& file, bool with_boi) override;

  /**
   * @brief Deserialize (load data) from a file.
   *
   * This class is a simple wrapper for a piece of contiguous memory. So this method just simply read
   * all float numbers from a file.
   *
   * @param file
   * @param endianness
   */
  void Deserialize(File& file, endian::Endianness endianness) override;
};


class SimulationRayData : public ISerializable {
 public:
  WavelengthInfo wavelength_info_{};

  void Clear();
  void PrepareNewScatter(size_t ray_num);
  void AddRay(RayInfo* ray);

  SimpleRayData CollectFinalRayData() const;

  void AddExitRaySegment(RaySegment* r);
  const std::vector<RaySegment*>& GetLastExitRaySegments() const;
#ifdef FOR_TEST
  const std::vector<std::vector<RaySegment*>>& GetExitRaySegments() const;
#endif

  /**
   * @brief Serialize self to a file.
   *
   * This class only holds pointers to ray segments and ray infos, rather than objects themselves.
   * To serialize data completely, this method will serialize ray segment pool and ray info
   * pool first.
   *
   * The layout of file is:
   * ray info pool,         // ray info pool
   * ray seg pool,          // ray seg pool
   * uint32,                // multi-scatters, K
   * {
   *   uint32,              // ray numbers, N
   *   (uint32 * 2) * N,    // ray info pointers
   * } * K
   * {
   *   uint32,              // ray seg numbers, N
   *   (uint32 * 2) * N,    // ray seg pointers
   * } * K
   *
   * @param file
   * @param with_boi
   */
  void Serialize(File& file, bool with_boi) override;

  /**
   * @brief Deserialize (load data) from a file.
   *
   * This class only holds pointers to ray segments and ray infos, rather than objects themselves.
   * To load data correctly, this method will deserialize ray segment pool and ray info
   * pool first, i.e. it will clear all existing data in ray segment pool and ray info pool.
   *
   * @warning It will clear all existing data in ray segment pool and ray info pool.
   *
   * @param file
   * @param endianness
   */
  void Deserialize(File& file, endian::Endianness endianness) override;

 private:
  std::vector<std::vector<RayInfo*>> rays_;
  std::vector<std::vector<RaySegment*>> exit_ray_segments_;
};

class Simulator {
 public:
  explicit Simulator(ProjectContextPtr context);
  Simulator(const Simulator& other) = delete;

  void SetCurrentWavelengthIndex(int index);
  void Run();
  const SimulationRayData& GetSimulationRayData();

  void SaveFinalDirections(const char* filename);
#ifdef FOR_TEST
  void PrintRayInfo();  // For debug
#endif

 private:
  struct EntryRayData {
    EntryRayData();
    ~EntryRayData();

    void Clear();
    void Allocate(size_t ray_number);

    float* ray_dir;
    RaySegment** ray_seg;
    size_t ray_num;
    size_t buf_size;
  };


  struct BufferData {
   public:
    BufferData();
    ~BufferData();

    void Clear();
    void Allocate(size_t ray_number);
    void Print();

    float* pt[2];
    float* dir[2];
    float* w[2];
    int* face_id[2];
    RaySegment** ray_seg[2];

    size_t ray_num;

   private:
    void DeleteBuffer(int idx);
  };


  static void InitMainAxis(const CrystalContext* ctx, float* axis);

  void InitSunRays();
  void InitEntryRays(const CrystalContext* ctx);
  void TraceRays(const Crystal* crystal, AbstractRayPathFilter* filter);
  void PrepareMultiScatterRays(float prob);
  void StoreRaySegments(const Crystal* crystal, AbstractRayPathFilter* filter);
  void RefreshBuffer();

  static constexpr int kBufferSizeFactor = 4;

  ProjectContextPtr context_;

  SimulationRayData simulation_ray_data_;

  int current_wavelength_index_;

  size_t total_ray_num_;
  size_t active_ray_num_;
  size_t buffer_size_;

  BufferData buffer_;
  EntryRayData entry_ray_data_;
  size_t entry_ray_offset_;
};

}  // namespace icehalo

#endif  // SRC_CORE_SIMULATION_H_