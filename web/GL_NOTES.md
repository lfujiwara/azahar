# OpenGL renderer on WebGL2: notes

Branch `wasm-gl`. The OpenGL renderer (`graphics_api=1`) runs on WebGL2 (GLES 3.0, GLSL `300 es`)
with both hardware and software vertex shading. `IsWebGL` in `gl_vars.h` is the compile-time switch.

## Run it

- `./web/build.sh`, then `python3 web/serve.py 8303`.
- `web/glsmoke.sh <rom> [api] [seconds] [settings]` runs `smoke.mjs` on port 8303 and prints the
  stats line plus the distinct error lines. Bare setting names get a `Renderer.` prefix, for
  example `./glsmoke.sh fragment_light 1 12 use_hw_shader=0,resolution_factor=2`.
- `smoke.mjs` reads `PORT` (default 8302) and logs page-error stacks. The wasm links with
  `--profiling-funcs`, so those stacks show C++ names.

## How each WebGL gap is handled

| Gap | Handling | Where |
| --- | --- | --- |
| `#version 320 es` | `#version 300 es`, plus default `highp` in fragment shaders and `fma` as a macro | `gl_shader_util.cpp` |
| `layout(binding=N)` | Stripped from the source. After link, `glUniform1i` sets each sampler and `glUniformBlockBinding` sets each UBO | `PatchForWebGL`, `LoadProgram` |
| `layout(location)` on uniforms, VS outputs and FS inputs | Stripped. Varyings now match by name, so `full_screen_triangle.vert` renamed `texcoord` to `tex_coord` | same |
| `glProgramUniform*` and explicit uniform locations | Blit helper sets uniforms by name (`SetUniform`) | `gl_blit_helper.cpp` |
| `out gl_PerVertex {}` block | Removed | patcher |
| `samplerBuffer` / `glTexBuffer` LUTs | 2D float textures (RG32F/RGBA32F, 1024 texels per row). `texelFetchLut` turns the linear index into `ivec2`. After each LUT sync, the touched rows are copied from the stream buffer's CPU shadow | `gl_rasterizer.cpp`, patcher |
| `glBufferStorage`, unsynchronized `glMapBufferRange` | Stream buffers write to a CPU shadow, then `glBufferSubData`. Emscripten's `glMapBufferRange` rejects `MAP_UNSYNCHRONIZED` | `gl_stream_buffer.cpp` |
| Separable programs and pipelines | Already off for GLES (`!driver.IsOpenGLES()`). Each VS+GS+FS combination links into one program | existing code |
| Geometry shaders | Hardware draws that need the fixed quaternion GS (fragment lighting) return false and fall back to software vertex shading | `SetupGeometryShader` |
| Image load/store (shadow maps) | Image bindings are skipped. Shadow-rendering draws are dropped, so shadow maps stay empty | `gl_state.cpp`, `RasterizerOpenGL::Draw` |
| `glDrawRangeElementsBaseVertex` | Indices are rebased on the CPU, then `glDrawElements` | `AccelerateDrawBatchInternal` |
| `glCopyImageSubData` in `CopyTextures` | Same-size nearest `BlitTextures` | `gl_texture_runtime.cpp` |
| `GL_CLAMP_TO_BORDER`, border color | Mapped to clamp-to-edge. The border color is ignored | `Sampler` |
| `glObjectLabel`, program binaries | Labels skipped. The disk shader cache is off on WebGL | runtime, `gl_shader_disk_cache.cpp` |
| `GL_MAX_TEXTURE_BUFFER_SIZE` query | Not queried. The fixed 2 MiB is used | `TextureBufferSize` |

## Findings

- The first null-function crash came from `OpenGLState::Apply` calling `glBindImageTexture`. The
  rasterizer always binds the null surface as the shadow image, even with no shadow draw.
- `glActiveTexture` is not tracked by `OpenGLState`, and `MakeHandle` binds new textures on
  whatever unit is active. LUT helpers leave unit 15 active so they never clobber LUT bindings.
- In this headless setup (SwiftShader), every ROM in `web/roms/` renders without errors. The
  static ROMs give the same lit fraction under GL and software. The animated ones (cube, lenny,
  fragment_light) match by eye once you allow for animation time.

## Still missing (not hit by the test ROMs)

- `Surface::CopyHandle` still calls `glCopyImageSubData` (a framebuffer-feedback copy).
- Shadow maps (needs image load/store) and the border color.
- `GL_EXT_clip_cull_distance`: user clip planes are unsupported. WebGL has
  `WEBGL_clip_cull_distance`, which is not wired up.
- `frame_dumper_opengl` (`glMapBuffer`) and `renderer_debug` (debug callbacks and groups).
