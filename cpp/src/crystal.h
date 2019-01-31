#ifndef SRC_CRYSTAL_H_
#define SRC_CRYSTAL_H_

#include "mymath.h"

#include <vector>
#include <memory>

namespace IceHalo {

class Crystal {
public:
  Crystal(const std::vector<Math::Vec3f>& vertexes, const std::vector<Math::TriangleIdx>& faces);
  Crystal(const std::vector<Math::Vec3f>& vertexes, const std::vector<Math::TriangleIdx>& faces,
          const std::vector<int>& faceId);

  int vtxNum() const;
  int faceNum() const;
  int faceId(int idx) const;

  const std::vector<Math::Vec3f>& getVertexes();
  const std::vector<Math::Vec3f>& getNorms();
  const std::vector<Math::TriangleIdx>& getFaces();

  void copyVertexData(float* data) const;
  void copyFaceData(float* data) const;
  void copyFaceIdxData(int* data) const;
  void copyNormalData(int idx, float* data) const;
  void copyNormalData(float* data) const;

  static constexpr float kC = 1.629f;

  /* Regular hexagon cylinder */
  static std::shared_ptr<Crystal> createHexCylinder(float h);

  /* Regular hexagon pyramid */
  static std::shared_ptr<Crystal> createHexPyramid(float h1, float h2, float h3);
  static std::shared_ptr<Crystal> createHexPyramid(int i1, int i4, float h1, float h2, float h3);
  static std::shared_ptr<Crystal> createHexPyramid(
    int upperIdx1, int upperIdx4, int lowerIdx1, int lowerIdx4, float h1, float h2, float h3);

  /* Hexagon stacked pyramid */
  static std::shared_ptr<Crystal> createHexPyramidStackHalf(
    int upperIdx1, int upperIdx4, int lowerIdx1, int lowerIdx4, float h1, float h2, float h3);

  /* Triangle pyramid */
  static std::shared_ptr<Crystal> createTriPyramid(int i1, int i4, float h1, float h2, float h3);

  /* Cubic pyramid (crystal type of Ic) */
  static std::shared_ptr<Crystal> createCubicPyramid(float ratio1, float ratio2);

  /* Irregular hexagon cylinder */
  static std::shared_ptr<Crystal> createIrregularHexCylinder(float* dist, float h);

  /* Irregular hexagon pyramid */
  static std::shared_ptr<Crystal> createIrregularHexPyramid(float* dist, int* idx, float* h);

protected:
  void initNorms();

private:
  std::vector<Math::Vec3f> vertexes;
  std::vector<Math::Vec3f> norms;
  std::vector<Math::TriangleIdx> faces;
  std::vector<int> faceIdMap;
};

using CrystalPtr = std::shared_ptr<Crystal>;

};  // namespace IceHalo


#endif  // SRC_CRYSTAL_H_
