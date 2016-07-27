#include <ze/pangolin/pangolin.hpp>

namespace ze {

PangolinPlotter::PangolinPlotter(const std::string& window_title,
                                 int width,
                                 int height)
  : window_title_(window_title)
  , width_(width)
  , height_(height)
{
  // Thread out a pangolin loop.
  thread_.reset(new std::thread(&PangolinPlotter::loop, this));
}

PangolinPlotter::~PangolinPlotter()
{
  requestStop();
  thread_->join();
}

void PangolinPlotter::loop()
{
  // Create OpenGL window and switch to context.
  pangolin::CreateWindowAndBind(window_title_, width_, height_);

  // Create plotter and listen to data log.
  plotter_ = std::make_shared<pangolin::Plotter>(&data_log_);
  // plotter.SetBounds(0.0, 1.0, 0.0, 1.0);
  plotter_->Track("$i");

  pangolin::DisplayBase().AddDisplay(*plotter_);

  while(!pangolin::ShouldQuit())
  while(!isStopRequested())
  {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Swap frames and Process Events
    pangolin::FinishFrame();

    std::this_thread::sleep_for(std::chrono::milliseconds(40));
  }
}

void PangolinPlotter::requestStop()
{
  std::lock_guard<std::mutex> lock(stop_mutex_);
  stop_requested_ = true;
}

bool PangolinPlotter::isStopRequested()
{
   std::lock_guard<std::mutex> lock(stop_mutex_);
   return stop_requested_;
}

} // namespace ze
