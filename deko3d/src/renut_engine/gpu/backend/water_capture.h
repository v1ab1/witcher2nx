#pragma once

#include <cstddef>
#include <cstdint>

namespace plume {
struct RenderTexture;
struct RenderTextureView;
}

namespace renut::gpu {
struct PresentImage;
struct VideoState;

constexpr uint64_t kWaterCaptureShader = 0x22DFD94B0F97BCDCull;
constexpr uint64_t kAmbientCaptureShader = 0xBEA67AEE6795A005ull;
constexpr uint64_t kAmbientShadowCaptureShader = 0x5FCF0FEF678E0110ull;
constexpr uint64_t kSkyCaptureShader = 0x1CE03F6D8891FBEEull;

constexpr bool IsLightingCaptureShader(uint64_t hash) {
  return hash == kAmbientCaptureShader || hash == kAmbientShadowCaptureShader ||
         hash == kSkyCaptureShader;
}

// All entry points run under VideoState::mutex, like the surrounding draw code.
void TrackWaterCaptureTextureLocked(uint32_t slot, const plume::RenderTexture *texture,
                                    const plume::RenderTextureView *view);
bool BeginWaterCaptureDrawLocked(uint64_t vs, uint64_t ps, const void *device, size_t device_size,
                                 const void *shared, size_t shared_size,
                                 const plume::RenderTexture *colour);
void CaptureLightingDrawLocked(uint64_t ps, bool after, const void *device, size_t device_size,
                               const void *shared, size_t shared_size,
                               const plume::RenderTexture *colour);
void EndWaterCaptureDrawLocked(const plume::RenderTexture *colour);
void FinishWaterCaptureFrameLocked(VideoState &state, const PresentImage *image);

} // namespace renut::gpu
