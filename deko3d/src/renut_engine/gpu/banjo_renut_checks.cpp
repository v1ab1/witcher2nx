// banjo-nx (package 27c): QueueKickFlush. See banjo_renut_checks.h.
#include "renut_engine/gpu/banjo_renut_checks.h"

#include <chrono>
#include <cstdint>

#include <rex/cvar.h>

#include "renut_engine/gpu/backend/render_thread.h"
#include "renut_engine/gpu/backend/video.h"
#include "renut_engine/gpu/log.h"
#include "renut_engine/gpu/render/draw.h"
#include "renut_engine/gpu/render/textures.h"

REXCVAR_DEFINE_BOOL(witcher2_diag_kick_path, false, "Witcher 2/Diagnostics",
                    "Trace where guest graphics work becomes safe to send to the GPU")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace banjo::renut_gpu::checks {

bool KickPathTraceOn() {
  static const bool enabled = REXCVAR_GET(witcher2_diag_kick_path);
  return enabled;
}

void QueueKickFlush(KickOrigin origin) {
  using Clock = std::chrono::steady_clock;
  using namespace ::renut::gpu;
  const auto queued = KickPathTraceOn() ? Clock::now() : Clock::time_point{};
  render_thread::Queue([origin, queued] {
    // Count from the game's Swap markers. Overlay-only presents do not advance this count.
    static uint64_t frame = 1;
    static Clock::time_point last_swap;
    static uint32_t device_kicks = 0, callbacks = 0;
    const bool trace = queued != Clock::time_point{};
    const bool sample = trace && (frame % 32 == 0);
    const auto started = sample ? Clock::now() : Clock::time_point{};
    const auto gpu_at_start = sample ? Video::PreviousFrameGpuDoneLocked() : std::nullopt;
    const auto &s = State();
    const uint32_t draws = sample && s.list_open ? s.list_draws : 0;
    const auto recording = sample && s.list_open ? s.frame_recording_start : Clock::time_point{};

    FlushVertexFillsLocked();
    FlushTextureFillsLocked();
    const auto ready = sample ? Clock::now() : Clock::time_point{};
    const auto gpu_at_ready = sample ? Video::PreviousFrameGpuDoneLocked() : std::nullopt;
    const bool submitted = Video::KickLocked();

    if (sample) {
      const auto finished = Clock::now();
      const auto ms = [](Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); };
      const auto gpu_state = [](std::optional<bool> done) {
        return !done ? "untracked" : *done ? "done" : "busy";
      };
      const char *kind = origin == KickOrigin::Device ? "device" : origin == KickOrigin::Callback ? "callback" : "swap";
      RGPU_INFO("[kick-path] frame {} {}: {} draws; queued {:.3f} ms after last swap; recording {:.3f} ms, "
                "queue {:.3f} ms, fills {:.3f} ms, kickoff {:.3f} ms; prior GPU {}/{}; sent {}",
                frame, kind, draws, last_swap == Clock::time_point{} ? 0.0 : ms(queued - last_swap),
                recording == Clock::time_point{} ? 0.0 : ms(started - recording), ms(started - queued),
                ms(ready - started), ms(finished - ready), gpu_state(gpu_at_start), gpu_state(gpu_at_ready), submitted);
      if (origin == KickOrigin::Swap)
        RGPU_INFO("[kick-path] frame {} before swap: {} device kicks, {} callback markers", frame, device_kicks, callbacks);
    }

    if (trace) {
      switch (origin) {
        case KickOrigin::Device: ++device_kicks; break;
        case KickOrigin::Callback: ++callbacks; break;
        case KickOrigin::Swap:
          last_swap = queued;
          ++frame;
          device_kicks = callbacks = 0;
          break;
      }
    }
  });
}

}  // namespace banjo::renut_gpu::checks
