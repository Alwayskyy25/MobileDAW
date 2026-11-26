// gles_renderer.h - draws a ui_core DrawList with OpenGL ES 2 (Android only).
// Everything (rects and font pixels) becomes one batch of coloured triangles.
#pragma once
#ifdef __ANDROID__
#include <vector>
#include "ui_core.h"

namespace mdnative {

// Letterbox transform between surface pixels and the 1280x720 logical space.
struct ViewTransform {
  float scale = 1, offX = 0, offY = 0;
  static ViewTransform fit(int surfW, int surfH);
  void toLogical(float px, float py, float* lx, float* ly) const {
    *lx = (px - offX) / scale;
    *ly = (py - offY) / scale;
  }
};

class GlesRenderer {
 public:
  bool init();           // compile shaders (call once, GL context current)
  void destroy();
  void draw(const std::vector<DrawCmd>& list, int surfW, int surfH);

 private:
  unsigned int prog_ = 0, vbo_ = 0;
  int locPos_ = -1, locColor_ = -1;
  std::vector<float> verts_;  // x,y, r,g,b,a per vertex
  void pushRect(float x, float y, float w, float h, uint32_t color, const ViewTransform& t, int surfW, int surfH);
};

}  // namespace mdnative
#endif  // __ANDROID__
