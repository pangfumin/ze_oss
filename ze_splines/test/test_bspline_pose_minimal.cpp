// Copyright (c) 2015-2016, ETH Zurich, Wyss Zurich, Zurich Eye
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the ETH Zurich, Wyss Zurich, Zurich Eye nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL ETH Zurich, Wyss Zurich, Zurich Eye BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <ze/splines/bspline_pose_minimal.hpp>

// Bring in gtest
#include <ze/common/test_entrypoint.hpp>
#include <ze/common/numerical_derivative.hpp>
#include <ze/common/transformation.hpp>
#include <ze/splines/operators.hpp>

namespace ze {

// A wrapper around a bspline to numerically estimate Jacobians at a given
// instance of time and derivative order
template<class ROTATION>
class FixedTimeBSplinePoseMinimal
{
public:
  //! t: time of evaluation
  //! d: derivative order (0..splineOrder -1)
  //! v: homogeneous coordinates vector to transform (transformation Jacobians)
  FixedTimeBSplinePoseMinimal(
      BSplinePoseMinimal<ROTATION>* bs,
      real_t t, int d, Vector4 v = Vector4::Zero())
    : bs_(bs)
    , t_(t)
    , d_(d)
    , v_(v)
  {
  }

  //! get the vector of active coefficients at the evaluation time
  VectorX coefficientVector()
  {
    return bs_->localCoefficientVector(t_);
  }

  //! set the coefficient vector at the evaluation time
  void setCoefficientVector(VectorX c)
  {
    return bs_->setLocalCoefficientVector(t_, c);
  }

  //! evaluate the splines given a local coefficient vector
  VectorX eval(VectorX c)
  {
    VectorX old_c = coefficientVector();
    setCoefficientVector(c);
    VectorX value = bs_->evalD(t_, d_);
    setCoefficientVector(old_c);

    return value;
  }

  //! evaluate the transformation given a local coefficient vector
  //! strictly it is T * v that is evaluated and not T itself
  Vector4 transformation(VectorX c)
  {
    VectorX old_c = coefficientVector();
    setCoefficientVector(c);
    Matrix4 value = bs_->transformation(t_);
    setCoefficientVector(old_c);

    return value * v_;
  }

  //! evaluate the inverse transformation given a local coefficient vector
  //! strictly it is T^1 * v that is evaluated and not T itself
  Vector4 inverseTransformation(VectorX c)
  {
    VectorX old_c = coefficientVector();
    setCoefficientVector(c);
    Matrix4 value = bs_->inverseTransformation(t_);
    setCoefficientVector(old_c);

    return value * v_;
  }

  Vector3 linearAcceleration(VectorX c)
  {
    VectorX old_c = coefficientVector();
    setCoefficientVector(c);
    Vector3 value = bs_->linearAccelerationAndJacobian(t_, NULL, NULL);
    setCoefficientVector(old_c);

    return value;
  }

  Vector3 angularVelocity(VectorX c)
  {
    VectorX old_c = coefficientVector();
    setCoefficientVector(c);
    Vector3 value = bs_->angularVelocity(t_);
    setCoefficientVector(old_c);

    return value;
  }

  Vector3 angularVelocityBodyFrame(VectorX c)
  {
    VectorX old_c = coefficientVector();
    setCoefficientVector(c);
    Vector3 value = bs_->angularVelocityBodyFrame(t_);
    setCoefficientVector(old_c);

    return value;
  }

private:
  BSplinePoseMinimal<ROTATION>* bs_;
  real_t t_;
  int d_;
  Vector4 v_;
};
} // namespace ze

TEST(BSplinePoseMinimalTestSuite, testCurveValueToTransformation)
{
  using namespace ze;

  BSplinePoseMinimal<ze::sm::RotationVector> bs(3);

  Vector6 point = Vector6::Random();
  Matrix4 T = bs.curveValueToTransformation(point);

  EXPECT_TRUE(EIGEN_MATRIX_NEAR(
                bs.transformationToCurveValue(T),
                point,
                1e-6));
}

// Check that the Jacobian calculation is correct.
TEST(BSplinePoseMinimalTestSuite, testBSplineTransformationJacobian)
{
  using namespace ze;

  for (int order = 2; order < 10; ++order)
  {
    // Create a two segment spline.
    BSplinePoseMinimal<ze::sm::RotationVector> bs(order);
    bs.initPoseSpline(0.0, 1.0, bs.curveValueToTransformation(VectorX::Random(6)),
                      bs.curveValueToTransformation(VectorX::Random(6)));
    bs.addPoseSegment(2.0,bs.curveValueToTransformation(VectorX::Random(6)));

    // Create a random homogeneous vector.
    Vector4 v = Vector4::Random() * 10.0;

    for (real_t t = bs.t_min(); t <= bs.t_max(); t+= 0.413)
    {
      FixedTimeBSplinePoseMinimal<ze::sm::RotationVector> fixed_bs(
            &bs, t, 0, v);

      Eigen::Matrix<real_t, Eigen::Dynamic, 1> point =
          fixed_bs.coefficientVector();
      MatrixX estJ =
          numericalDerivative<MatrixX, VectorX>(
            std::bind(
              &FixedTimeBSplinePoseMinimal<ze::sm::RotationVector>::transformation,
              &fixed_bs, std::placeholders::_1), point);

      MatrixX JT;
      Matrix4 T = bs.transformationAndJacobian(t, &JT);

      MatrixX J = ze::sm::boxMinus(T*v) * JT;

      EXPECT_TRUE(EIGEN_MATRIX_NEAR(J, estJ, 1e-6));

      // Try again with the lumped function.
      Vector4 v_n = bs.transformVectorAndJacobian(t, v, &J);
      EXPECT_TRUE(EIGEN_MATRIX_NEAR(v_n, T*v, 1e-6));
      EXPECT_TRUE(EIGEN_MATRIX_NEAR(J, estJ, 1e-6));

      return;
    }
  }
}

// Check that the Jacobian calculation is correct.
TEST(BSplinePoseMinimalTestSuite, testBSplineInverseTransformationJacobian)
{
  using namespace ze;

  for (int order = 2; order < 10; ++order) {
    // Create a two segment spline.
    BSplinePoseMinimal<ze::sm::RotationVector> bs(order);
    bs.initPoseSpline(0.0, 1.0, bs.curveValueToTransformation(VectorX::Random(6)),
                      bs.curveValueToTransformation(VectorX::Random(6)));
    bs.addPoseSegment(2.0,bs.curveValueToTransformation(VectorX::Random(6)));

    // Create a random homogeneous vector.
    Vector4 v = Vector4::Random() * 10.0;

    for (real_t t = bs.t_min(); t <= bs.t_max(); t+= 0.413) {
      FixedTimeBSplinePoseMinimal<ze::sm::RotationVector> fixed_bs(
            &bs, t, 0, v);

      Eigen::Matrix<real_t, Eigen::Dynamic, 1> point =
          fixed_bs.coefficientVector();
      MatrixX estJ =
          numericalDerivative<MatrixX, VectorX>(
            std::bind(
              &FixedTimeBSplinePoseMinimal<ze::sm::RotationVector>::inverseTransformation,
              &fixed_bs, std::placeholders::_1), point);

      MatrixX JT;
      MatrixX J;

      Matrix4 T = bs.inverseTransformationAndJacobian(t, &JT);

      J = ze::sm::boxMinus(T*v) * JT;
      EXPECT_TRUE(EIGEN_MATRIX_NEAR(J, estJ, 1e-6));
    }
  }
}

TEST(BSplinePoseMinimalTestSuite, testBSplineAccelerationJacobian)
{
  using namespace ze;

  for (int order = 2; order < 10; ++order) {
    // Create a two segment spline.
    BSplinePoseMinimal<ze::sm::RotationVector> bs(order);
    bs.initPoseSpline(0.0, 1.0, bs.curveValueToTransformation(VectorX::Random(6)),
                      bs.curveValueToTransformation(VectorX::Random(6)));
    bs.addPoseSegment(2.0,bs.curveValueToTransformation(VectorX::Random(6)));

    for (real_t t = bs.t_min(); t <= bs.t_max(); t+= 0.1) {
      MatrixX J;
      bs.linearAccelerationAndJacobian(t, &J, NULL);

      FixedTimeBSplinePoseMinimal<ze::sm::RotationVector> fixed_bs(
            &bs, t, 0);

      Eigen::Matrix<real_t, Eigen::Dynamic, 1> point =
          fixed_bs.coefficientVector();
      MatrixX estJ =
          numericalDerivative<VectorX, VectorX>(
            std::bind(
              &FixedTimeBSplinePoseMinimal<ze::sm::RotationVector>::linearAcceleration,
              &fixed_bs, std::placeholders::_1), point);

      EXPECT_TRUE(EIGEN_MATRIX_NEAR(J, estJ, 1e-6));

    }
  }
}

TEST(BSplinePoseMinimalTestSuite, testBSplineAngularVelocityJacobian)
{
  using namespace ze;

  for (int order = 2; order < 10; ++order)
  {
    // Create a two segment spline.
    BSplinePoseMinimal<ze::sm::RotationVector> bs(order);
    bs.initPoseSpline(0.0, 1.0, bs.curveValueToTransformation(VectorX::Random(6)),
                      bs.curveValueToTransformation(VectorX::Random(6)));
    bs.addPoseSegment(2.0,bs.curveValueToTransformation(VectorX::Random(6)));

    for (real_t t = bs.t_min(); t <= bs.t_max(); t+= 0.1) {
      MatrixX J;
      bs.angularVelocityAndJacobian(t, &J, NULL);

      FixedTimeBSplinePoseMinimal<ze::sm::RotationVector> fixed_bs(
            &bs, t, 0);

      Eigen::Matrix<real_t, Eigen::Dynamic, 1> point =
          fixed_bs.coefficientVector();
      MatrixX estJ =
          numericalDerivative<VectorX, VectorX>(
            std::bind(
              &FixedTimeBSplinePoseMinimal<ze::sm::RotationVector>::angularVelocity,
              &fixed_bs, std::placeholders::_1), point);

      // opposite sign due to perturbation choice
      EXPECT_TRUE(EIGEN_MATRIX_NEAR(J, -estJ, 1e-6));

    }
  }
}

TEST(BSplinePoseMinimalTestSuite, testBSplineAngularVelocityBodyFrameJacobian)
{
  using namespace ze;

  for (int order = 2; order < 10; ++order) {
    // Create a two segment spline.
    BSplinePoseMinimal<ze::sm::RotationVector> bs(order);
    bs.initPoseSpline(0.0, 1.0, bs.curveValueToTransformation(VectorX::Random(6)),
                      bs.curveValueToTransformation(VectorX::Random(6)));
    bs.addPoseSegment(2.0,bs.curveValueToTransformation(VectorX::Random(6)));

    for (real_t t = bs.t_min(); t <= bs.t_max(); t+= 0.1)
    {
      MatrixX J;
      bs.angularVelocityBodyFrameAndJacobian(t, &J, NULL);

      FixedTimeBSplinePoseMinimal<ze::sm::RotationVector> fixed_bs(
            &bs, t, 0);

      Eigen::Matrix<real_t, Eigen::Dynamic, 1> point =
          fixed_bs.coefficientVector();
      MatrixX estJ =
          numericalDerivative<VectorX, VectorX>(
            std::bind(
              &FixedTimeBSplinePoseMinimal<ze::sm::RotationVector>::angularVelocityBodyFrame,
              &fixed_bs, std::placeholders::_1), point);

      EXPECT_TRUE(EIGEN_MATRIX_NEAR(J, estJ, 1e-6));

    }
  }
}

TEST(BSplinePoseMinimalTestSuite, testInitializePoses)
{
  using namespace ze;

  BSplinePoseMinimal<ze::sm::RotationVector> bs(3);
  bs.initPoseSpline(0.0, 1.0, bs.curveValueToTransformation(VectorX::Random(6)),
                    bs.curveValueToTransformation(VectorX::Random(6)));
  bs.addPoseSegment(2.0,bs.curveValueToTransformation(VectorX::Random(6)));

  // get a vector matrice
  std::vector<Matrix4> poses;
  Eigen::Matrix<real_t, 1, 20> times;
  size_t i = 0;
  for (real_t t = bs.t_min(); t <= bs.t_max(); t += 0.1)
  {
    times(i) = t;
    poses.push_back(bs.transformation(t));
    ++i;
  }

  // init another spline
  BSplinePoseMinimal<ze::sm::RotationVector> bs2(3);
  bs2.initPoseSplinePoses(times, poses, 8, 1e-6);

  // compare
  for (real_t t = bs.t_min(); t <= bs.t_max(); t += 0.1)
  {
    EXPECT_TRUE(EIGEN_MATRIX_NEAR(
                  bs.transformation(t),
                  bs2.transformation(t),
                  1e-2));
  }
}

ZE_UNITTEST_ENTRYPOINT
