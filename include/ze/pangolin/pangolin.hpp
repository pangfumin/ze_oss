#pragma once

#include <memory>
#include <mutex>
#include <thread>
#include <pangolin/pangolin.h>

namespace ze {

//! A class to continuously plot a series of measurements into a window.
class PangolinPlotter
{
public:
  //! Creates a new pangolin window
  PangolinPlotter(const std::string& window_title = "",
                  int width = 640,
                  int height = 480);
  ~PangolinPlotter();

  //! Log some data.
  template<typename Scalar>
  void log(Scalar value)
  {
    data_log_.Log(value);
  }

  //! A threaded visualization loop.
  void loop();

private:
  //! The window title, also used as window context.
  const std::string window_title_;
  const int width_;
  const int height_;

  //! A data logger object to send data to.
  pangolin::DataLog data_log_;

  std::shared_ptr<pangolin::Plotter> plotter_;

  std::unique_ptr<std::thread> thread_;
  std::mutex stop_mutex_;
  bool stop_requested_ = false;

  void requestStop();
  bool isStopRequested();
};

} // namespace ze
