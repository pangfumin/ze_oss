#include <gtest/gtest.h>

// system includes
#include <assert.h>
#include <cstdint>
#include <cfloat>
#include <iostream>
#include <random>
#include <functional>
#include <limits>

#include <imp/core/image_raw.hpp>
#include <imp/cu_core/cu_math.cuh>
#include <imp/cu_core/cu_utils.hpp>
#include <ze/common/test_utils.h>


TEST(IMPCuCoreTestSuite,minMaxTest_8uC1)
{
  auto random_val = ze::getRandomGenerator<std::uint8_t>();

  size_t width = 123;
  size_t height = 324;
  ze::ImageRaw8uC1 im(width,height);
  std::uint8_t min_val = std::numeric_limits<std::uint8_t>::max();
  std::uint8_t max_val = std::numeric_limits<std::uint8_t>::lowest();
  for (size_t y=0; y<height; ++y)
  {
    for (size_t x=0; x<width; ++x)
    {
      std::uint8_t random_value = random_val();
      im[y][x] = random_value;
      min_val = ze::cu::min(min_val, random_value);
      max_val = ze::cu::max(max_val, random_value);
    }
  }

  IMP_CUDA_CHECK();
  ze::cu::ImageGpu8uC1 cu_im(im);
  IMP_CUDA_CHECK();
  ze::Pixel8uC1 min_pixel, max_pixel;
  IMP_CUDA_CHECK();
  ze::cu::minMax(cu_im, min_pixel, max_pixel);
  IMP_CUDA_CHECK();

  ASSERT_EQ(min_val, min_pixel);
  ASSERT_EQ(max_val, max_pixel);
}


TEST(IMPCuCoreTestSuite,minMaxTest_32fC1)
{
  // setup random number generator
  auto random_val = ze::getRandomGenerator<float>();

  size_t width = 1250;
  size_t height = 325;
  ze::ImageRaw32fC1 im(width,height);
  float min_val = std::numeric_limits<float>::max();
  float max_val = std::numeric_limits<float>::lowest();
  for (size_t y=0; y<height; ++y)
  {
    for (size_t x=0; x<width; ++x)
    {
      float random_value = random_val();
      im[y][x] = random_value;
      min_val = ze::cu::min(min_val, random_value);
      max_val = ze::cu::max(max_val, random_value);

//      std::cout << "random: [" << x << "][" << y << "]: " << random_value << std::endl;
    }
  }

  std::cout << "numeric:  min, max: " << std::numeric_limits<float>::lowest() << " " << std::numeric_limits<float>::max() << std::endl;
  std::cout << "numeric2: min, max: " << FLT_MIN << " " << FLT_MAX << std::endl;
  std::cout << "CPU min, max: " << min_val << " " << max_val << std::endl;

  IMP_CUDA_CHECK();
  ze::cu::ImageGpu32fC1 cu_im(im);
  IMP_CUDA_CHECK();
  ze::Pixel32fC1 min_pixel, max_pixel;
  IMP_CUDA_CHECK();
  ze::cu::minMax(cu_im, min_pixel, max_pixel);
  IMP_CUDA_CHECK();

  std::cout << "GPU min, max: " << min_pixel << " " << max_pixel << std::endl;


  ASSERT_EQ(min_val, min_pixel.x);
  ASSERT_EQ(max_val, max_pixel.x);
}
