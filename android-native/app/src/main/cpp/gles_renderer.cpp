// gles_renderer.cpp - see gles_renderer.h.
#ifdef __ANDROID__
#include "gles_renderer.h"
#include <GLES2/gl2.h>
#include <cmath>
#include "font3x5.h"

namespace mdnative {

ViewTransform ViewTransform::fit(int surfW, int surfH) {
  ViewTransform t;
  t.scale = std::fmin(surfW / App::kDesignW, surfH / App::kDesignH);
  t.offX = (surfW - App::kDesignW * t.scale) / 2;
  t.offY = (surfH - App::kDesignH * t.scale) / 2;
  return t;
}

static const char* VS =
    "attribute vec2 aPos; attribute vec4 aColor; varying vec4 vColor;\n"
    "void main(){ vColor=aColor; gl_Position=vec4(aPos,0.0,1.0); }";
static const char* FS =
    "precision mediump float; varying vec4 vColor;\n"
    "void main(){ gl_FragColor=vColor; }";

static GLuint compile(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  return s;
}

bool GlesRenderer::init() {
  prog_ = glCreateProgram();
  glAttachShader(prog_, compile(GL_VERTEX_SHADER, VS));
  glAttachShader(prog_, compile(GL_FRAGMENT_SHADER, FS));
  glLinkProgram(prog_);
  locPos_ = glGetAttribLocation(prog_, "aPos");
  locColor_ = glGetAttribLocation(prog_, "aColor");
  glGenBuffers(1, &vbo_);
  return prog_ != 0;
}

void GlesRenderer::destroy() {
  if (vbo_) glDeleteBuffers(1, &vbo_);
  if (prog_) glDeleteProgram(prog_);
  vbo_ = prog_ = 0;
}

void GlesRenderer::pushRect(float x, float y, float w, float h, uint32_t c, const ViewTransform& t, int W, int H) {
  // logical -> pixels -> NDC (y flipped)
  float px0 = t.offX + x * t.scale, py0 = t.offY + y * t.scale;
  float px1 = px0 + w * t.scale, py1 = py0 + h * t.scale;
  float nx0 = px0 / W * 2 - 1, nx1 = px1 / W * 2 - 1;
  float ny0 = 1 - py0 / H * 2, ny1 = 1 - py1 / H * 2;
  float r = ((c >> 24) & 255) / 255.0f, g = ((c >> 16) & 255) / 255.0f,
        b = ((c >> 8) & 255) / 255.0f, a = (c & 255) / 255.0f;
  const float quad[6][2] = {{nx0, ny0}, {nx1, ny0}, {nx0, ny1}, {nx1, ny0}, {nx1, ny1}, {nx0, ny1}};
  for (auto& q : quad) {
    verts_.push_back(q[0]); verts_.push_back(q[1]);
    verts_.push_back(r); verts_.push_back(g); verts_.push_back(b); verts_.push_back(a);
  }
}

void GlesRenderer::draw(const std::vector<DrawCmd>& list, int surfW, int surfH) {
  if (!prog_ || surfW <= 0 || surfH <= 0) return;
  ViewTransform t = ViewTransform::fit(surfW, surfH);
  verts_.clear();
  glViewport(0, 0, surfW, surfH);
  glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  for (const DrawCmd& d : list) {
    if (d.kind == DrawCmd::FillRect) {
      pushRect(d.r.x, d.r.y, d.r.w, d.r.h, d.color, t, surfW, surfH);
    } else {  // Text: expand the 3x5 font into pixel rects
      float cx = d.r.x;
      float s = d.textScale;
      for (char ch : d.text) {
        const uint8_t* g = font3x5::glyph(ch);
        for (int row = 0; row < 5; row++)
          for (int col = 0; col < 3; col++)
            if (g[row] & (1 << (2 - col)))
              pushRect(cx + col * s, d.r.y + row * s, s * 0.9f, s * 0.9f, d.color, t, surfW, surfH);
        cx += 4 * s;
      }
    }
  }
  glUseProgram(prog_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, verts_.size() * sizeof(float), verts_.data(), GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(locPos_);
  glEnableVertexAttribArray(locColor_);
  glVertexAttribPointer(locPos_, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
  glVertexAttribPointer(locColor_, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
  glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts_.size() / 6));
  glDisableVertexAttribArray(locPos_);
  glDisableVertexAttribArray(locColor_);
}

}  // namespace mdnative
#endif  // __ANDROID__
