#pragma once

#include <ze/cameras/camera.h>
#include <ze/cameras/camera_models.h>

namespace ze {

template<class Distortion>
class PinholeProjection : public Camera
{
public:

  static constexpr DistortionType distortion_type = Distortion::type;

  //! Default constructor.
  using Camera::Camera;

  virtual ~PinholeProjection() = default;

  virtual Keypoint project(
      const Eigen::Ref<const Bearing>& bearing) const override
  {
    // Unit coordinates -> distortion -> pinhole, offset and scale.
    Keypoint px = bearing.head<2>() / bearing(2);
    Distortion::distort(this->distortion_params_.data(), px.data());
    PinholeGeometry::project(this->projection_params_.data(), px.data());
    return px;
  }

  virtual Bearing backProject(
      const Eigen::Ref<const Keypoint>& px) const override
  {
    Bearing bearing;
    bearing << px(0), px(1), 1.0;
    PinholeGeometry::backProject(this->projection_params_.data(), bearing.data());
    Distortion::undistort(this->distortion_params_.data(), bearing.data());
    return bearing.normalized();
  }

  virtual Matrix23 dProject_dLandmark(
      const Eigen::Ref<const Position>& pos) const override
  {
    Matrix22 J_dist;
    FloatType z_inv = 1.0 / pos.z();
    FloatType z_inv_sq = z_inv * z_inv;
    Keypoint px_unitplane = pos.head<2>() * z_inv;
    Distortion::distort(
          this->distortion_params_.data(), px_unitplane.data(), J_dist.data());
    const FloatType fx = this->projection_params_[0];
    const FloatType fy = this->projection_params_[1];
    Matrix23 J;
    J(0, 0) = fx * J_dist(0, 0) * z_inv;
    J(0, 1) = fx * J_dist(0, 1) * z_inv;
    J(0, 2) = -fx * (pos.x() * J_dist(0, 0) + pos.y() * J_dist(0, 1)) * z_inv_sq;
    J(1, 0) = fy * J_dist(1, 0) * z_inv;
    J(1, 1) = fy * J_dist(1, 1) * z_inv;
    J(1, 2) = -fy * (pos.x() * J_dist(1, 0) + pos.y() * J_dist(1, 1)) * z_inv_sq;
    return J;
  }

  std::pair<Keypoint, Matrix23> projectWithJacobian(
        const Eigen::Ref<const Position>& pos) const
  {
    //! @todo: project and jacobian computation do many duplicate things.
    Keypoint px = project(pos);
    Matrix23 J = dProject_dLandmark(pos);
    return std::make_pair(px, J);
  }

  virtual FloatType getApproxAnglePerPixel() const override
  {
    //! @todo: Is this correct? And if yes, this is costlty to compute often!
    //!        replace with acos and a dot product between the bearing vectors.
    // abs() because ICL-NUIM has negative focal length.
    return std::atan(1.0 / (2.0 * std::abs(this->projection_params_[0])))
         + std::atan(1.0 / (2.0 * std::abs(this->projection_params_[1])));
  }

  virtual FloatType getApproxBearingAngleFromPixelDifference(FloatType px_diff) const override
  {
    //! @todo: Is this correct? And if yes, this is costlty to compute often!
    //!        acos and a dot product between the bearing vectors.
    // abs() because ICL-NUIM has negative focal length.
    return std::atan(px_diff / (2.0 * std::abs(this->projection_params_[0])))
         + std::atan(px_diff / (2.0 * std::abs(this->projection_params_[1])));
  }
};

//-----------------------------------------------------------------------------
// Convenience typedefs.
// (sync with explicit template class instantiations at the end of the cpp file)
typedef PinholeProjection<NoDistortion> PinholeCamera;
typedef PinholeProjection<FovDistortion> FovCamera;
typedef PinholeProjection<RadialTangentialDistortion> RadTanCamera;
typedef PinholeProjection<EquidistantDistortion> EquidistantCamera;

//-----------------------------------------------------------------------------
// Convenience factory functions.

inline PinholeCamera createPinholeCamera(
    int width, int height, FloatType fx, FloatType fy, FloatType cx, FloatType cy)
{
  return PinholeCamera(width, height, CameraType::Pinhole,
                       (Vector4() << fx, fy, cx, cy).finished(), VectorX());
}

inline FovCamera createFovCamera(
    int width, int height, FloatType fx, FloatType fy, FloatType cx, FloatType cy,
    FloatType s)
{
  return FovCamera(width, height, CameraType::PinholeFov,
                   (Vector4() << fx, fy, cx, cy).finished(),
                   (Vector1() << s).finished());
}

inline RadTanCamera createRadTanCamera(
    int width, int height, FloatType fx, FloatType fy, FloatType cx, FloatType cy,
    FloatType k1, FloatType k2, FloatType r1, FloatType r2)
{
  return RadTanCamera(width, height, CameraType::PinholeRadialTangential,
                       (Vector4() << fx, fy, cx, cy).finished(),
                       (Vector4() << k1, k2, r1, r2).finished());
}

inline EquidistantCamera createEquidistantCamera(
    int width, int height, FloatType fx, FloatType fy, FloatType cx, FloatType cy,
    FloatType k1, FloatType k2, FloatType k3, FloatType k4)
{
  return EquidistantCamera(width, height, CameraType::PinholeEquidistant,
                           (Vector4() << fx, fy, cx, cy).finished(),
                           (Vector4() << k1, k2, k3, k4).finished());
}

} // namespace ze
