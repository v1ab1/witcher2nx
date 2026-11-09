#include "renut_engine/gpu/backend/water_capture.h"

#include "renut_engine/gpu/backend/backend.h"
#include "renut_engine/gpu/backend/video_internal.h"
#include "renut_engine/gpu/banjo_renut_tuning.h"

#include <rex/cvar.h>
#include <rex/logging.h>

#if defined(PLUME_DEKO3D) && defined(__SWITCH__)
#include <plume_deko3d.h>
#include <switch.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>

REXCVAR_DEFINE_BOOL(witcher2_water_capture_auto, false, "Witcher 2/Diagnostics",
                    "Record the first matching water draw after gameplay starts");

namespace renut::gpu {
namespace {

constexpr uint32_t kImageIndexMask = 0xFFFFF;
constexpr uint64_t kMaxBytes = 64u << 20;
// Albedo, normal/material, scene depth, shadow, noise, clouds, and AO.
constexpr std::array<uint32_t, 7> kAmbientSlots = {0, 1, 3, 4, 5, 6, 7};
constexpr uint32_t kLightingStages = (1u << (kAmbientSlots.size() + 4)) - 1;

struct TextureSlot {
  const plume::RenderTexture *texture;
  const plume::RenderTextureView *view;
};
std::array<TextureSlot, kTextureDescriptorCount> g_slots{};

enum class Phase : uint32_t { Idle, Control, Probe };
enum class Kind : uint32_t { Before = 1, After, Front = 4, AmbientInput, AmbientFirst,
                             AmbientSecond, BeforeSky, AfterSky };
struct ImageRecord {
  Kind kind;
  uint32_t guest_slot, descriptor, format, mip, layer;
  uint32_t width, height, row_bytes, rows, view_format, swizzle;
};
static_assert(sizeof(ImageRecord) == 48);

struct Image {
  ImageRecord record;
  std::unique_ptr<plume::RenderBuffer> bytes;
};

struct AmbientDraw {
  uint64_t shader;
  std::vector<uint8_t> device, shared;
};

struct Capture {
  Phase phase = Phase::Idle;
  bool drew = false;
  uint32_t missing = 0;
  uint32_t lighting_stages = 0;
  uint64_t vs = 0;
  uint64_t bytes = 0;
  std::vector<uint8_t> device, shared;
  std::vector<Image> images;
  std::vector<AmbientDraw> ambient;
} g_capture;

uint32_t ViewSwizzle(const plume::Deko3DTextureView *view) {
  uint32_t packed = 0;
  if (view) {
    for (int i = 0; i < 4; ++i)
      packed |= uint32_t(view->view.swizzle[i]) << (8 * i);
  }
  return packed;
}

bool RecordTexture(VideoState &s, Kind kind, const TextureSlot &slot,
                   uint32_t guest_slot = UINT32_MAX, uint32_t descriptor = UINT32_MAX) {
  if (!slot.texture)
    return false;
  const auto &tex = *static_cast<const plume::Deko3DTexture *>(slot.texture);
  const auto *view = static_cast<const plume::Deko3DTextureView *>(slot.view);
  const auto &desc = tex.desc;
  if (!tex.valid || desc.multisampling.sampleCount != plume::RenderSampleCount::COUNT_1 ||
      tex.type != DkImageType_2D)
    return false;
  using F = plume::RenderFormat;
  switch (desc.format) {
  case F::R16G16B16A16_FLOAT:
  case F::R16G16B16A16_UNORM:
  case F::R16G16B16A16_SNORM:
  case F::R8G8B8A8_UNORM:
  case F::B8G8R8A8_UNORM:
  case F::R32_FLOAT:
  case F::D32_FLOAT_S8_UINT:
  case F::D24_UNORM_S8_UINT:
  case F::BC1_UNORM:
  case F::BC1_UNORM_SRGB:
  case F::BC3_UNORM:
  case F::BC3_UNORM_SRGB:
    break;
  default:
    return false;
  }
  const uint32_t block = plume::RenderFormatBlockWidth(desc.format);
  const uint32_t block_bytes = plume::RenderFormatSize(desc.format);
  const uint32_t mips = kind == Kind::AmbientInput ? desc.mipLevels : 1;
  for (uint32_t mip = 0; mip < mips; ++mip) {
    const uint32_t width = std::max(1u, desc.width >> mip), height = std::max(1u, desc.height >> mip);
    const uint32_t rows = (height + block - 1) / block;
    const uint32_t row_bytes = (((width + block - 1) / block * block_bytes) + 255) & ~255u;
    const uint64_t size = uint64_t(rows) * row_bytes;
    if (size > kMaxBytes - g_capture.bytes)
      return false;
    auto buffer = s.device->createBuffer(plume::RenderBufferDesc::ReadbackBuffer(size));
    if (!buffer || !buffer->map())
      return false;
    buffer->unmap();
    const auto dest = plume::RenderTextureCopyLocation::PlacedFootprint(
        buffer.get(), desc.format, width, height, 1, row_bytes / block_bytes * block);
    const auto src = plume::RenderTextureCopyLocation::Subresource(&tex, mip, 0);
    // The readback records its own GPU barriers without rebinding graphics state.
    s.command_list->copyTextureRegion(dest, src);
    g_capture.images.push_back({{kind, guest_slot, descriptor, uint32_t(desc.format), mip, 0,
                                 width, height, row_bytes, rows,
                                 view ? uint32_t(view->view.format) : 0, ViewSwizzle(view)},
                                std::move(buffer)});
    g_capture.bytes += size;
  }
  return true;
}

bool WriteCapture() {
  const auto path = banjo::renut_gpu::NextToExecutable(
      g_capture.phase == Phase::Control ? "lighting-control.bin" : "lighting-probe.bin");
  auto temporary = path;
  temporary += ".tmp";
  FILE *file = std::fopen(temporary.string().c_str(), "wb");
  if (!file)
    return false;
  const auto write = [file](const void *data, size_t size) {
    return std::fwrite(data, 1, size, file) == size;
  };
  const uint32_t header[] = {3, uint32_t(g_capture.phase), g_capture.missing,
                             uint32_t(g_capture.images.size()), uint32_t(g_capture.device.size()),
                             uint32_t(g_capture.shared.size())};
  const uint64_t shaders[] = {g_capture.vs, kWaterCaptureShader};
  bool ok = write("W2WATER\0", 8) && write(header, sizeof(header)) &&
            write(shaders, sizeof(shaders)) && write(g_capture.device.data(), g_capture.device.size()) &&
            write(g_capture.shared.data(), g_capture.shared.size());
  for (const Image &image : g_capture.images) {
    if (!ok)
      break;
    const auto *bytes = image.bytes->map();
    ok = bytes && write(&image.record, sizeof(image.record)) &&
         write(bytes, size_t(image.record.row_bytes) * image.record.rows);
    if (bytes)
      image.bytes->unmap();
  }
  const uint32_t ambient_count = uint32_t(g_capture.ambient.size());
  ok = ok && write(&ambient_count, sizeof(ambient_count));
  for (const AmbientDraw &draw : g_capture.ambient) {
    ok = ok && write(&draw.shader, sizeof(draw.shader)) &&
         write(draw.device.data(), draw.device.size()) &&
         write(draw.shared.data(), draw.shared.size());
  }
  ok = (std::fclose(file) == 0) && ok;
  if (ok)
    ok = std::rename(temporary.string().c_str(), path.string().c_str()) == 0;
  if (!ok)
    std::remove(temporary.string().c_str());
  return ok;
}

} // namespace

void TrackWaterCaptureTextureLocked(uint32_t slot, const plume::RenderTexture *texture,
                                    const plume::RenderTextureView *view) {
  g_slots[slot] = {texture, view};
}

bool BeginWaterCaptureDrawLocked(uint64_t vs, uint64_t ps, const void *device, size_t device_size,
                                 const void *shared, size_t shared_size,
                                 const plume::RenderTexture *colour) {
  if (ps != kWaterCaptureShader || g_capture.phase == Phase::Idle || g_capture.drew || !colour)
    return false;
  g_capture.drew = true;
  g_capture.vs = vs;
  const auto *d = static_cast<const uint8_t *>(device);
  const auto *sc = static_cast<const uint8_t *>(shared);
  g_capture.device.assign(d, d + device_size);
  g_capture.shared.assign(sc, sc + shared_size);
  if (g_capture.phase == Phase::Probe && !RecordTexture(State(), Kind::Before, {colour, nullptr}))
    g_capture.missing |= 1u << 0;
  return true;
}

void EndWaterCaptureDrawLocked(const plume::RenderTexture *colour) {
  auto &s = State();
  if (!RecordTexture(s, Kind::After, {colour, nullptr}))
    g_capture.missing |= 1u << 1;
}

void CaptureLightingDrawLocked(uint64_t ps, bool after, const void *device, size_t device_size,
                               const void *shared, size_t shared_size,
                               const plume::RenderTexture *colour) {
  if (g_capture.phase != Phase::Probe || g_capture.drew || !colour)
    return;
  auto &s = State();
  if (ps == kSkyCaptureShader) {
    // Sky follows local lights. The pre-sky target is their combined output.
    const uint32_t bit = 1u << (kAmbientSlots.size() + (after ? 3 : 2));
    const Kind kind = after ? Kind::AfterSky : Kind::BeforeSky;
    if (!g_capture.ambient.empty() && !(g_capture.lighting_stages & bit) &&
        RecordTexture(s, kind, {colour, nullptr}))
      g_capture.lighting_stages |= bit;
    return;
  }
  if (!after || g_capture.ambient.size() >= 2)
    return;
  const bool first = g_capture.ambient.empty();
  const auto *d = static_cast<const uint8_t *>(device);
  const auto *sc = static_cast<const uint8_t *>(shared);
  g_capture.ambient.push_back({ps, {d, d + device_size}, {sc, sc + shared_size}});
  if (RecordTexture(s, first ? Kind::AmbientFirst : Kind::AmbientSecond, {colour, nullptr}))
    g_capture.lighting_stages |= 1u << (kAmbientSlots.size() + !first);
  if (!first)
    return;
  // Ambient lighting has already sampled these read-only inputs. The copies
  // cannot change the first ambient draw's output.
  for (uint32_t i = 0; i < kAmbientSlots.size(); ++i) {
    const uint32_t slot = kAmbientSlots[i];
    const size_t offset = slot * sizeof(uint32_t);
    if (offset + sizeof(uint32_t) > shared_size)
      continue;
    uint32_t id;
    std::memcpy(&id, sc + offset, sizeof(id));
    id &= kImageIndexMask;
    if (id >= kFirstDynamicTextureSlot && id < g_slots.size() &&
        RecordTexture(s, Kind::AmbientInput, g_slots[id], slot, id)) {
      g_capture.lighting_stages |= 1u << i;
    }
  }
}

void FinishWaterCaptureFrameLocked(VideoState &s, const PresentImage *image) {
  if (g_capture.drew) {
    if (g_capture.phase == Phase::Probe)
      g_capture.missing |= (kLightingStages & ~g_capture.lighting_stages) << 9;
    if (!image || !RecordTexture(s, Kind::Front, {image->texture, nullptr}))
      g_capture.missing |= 1u << 8;
    detail::SubmitAndWaitLocked(s);
    const bool ok = WriteCapture();
    REXLOG_INFO("[water] {} capture {}, {} images, missing mask 0x{:03x}",
                g_capture.phase == Phase::Control ? "control" : "probe",
                ok ? "saved" : "failed", g_capture.images.size(), g_capture.missing);
    const Phase next = ok && g_capture.phase == Phase::Control ? Phase::Probe : Phase::Idle;
    g_capture = {};
    g_capture.phase = next;
  } else if (!g_capture.ambient.empty()) {
    // Do not carry an earlier scene's samples into the next frame. The GPU
    // must finish the scheduled copies before their buffers can be freed.
    if (!g_capture.images.empty())
      detail::SubmitAndWaitLocked(s);
    g_capture = {};
    g_capture.phase = Phase::Probe;
  }

  static PadState pad;
  static const bool initialized = [] { padInitializeDefault(&pad); return true; }();
  (void)initialized;
  padUpdate(&pad);
  constexpr uint64_t chord = HidNpadButton_L | HidNpadButton_R | HidNpadButton_Y;
  const bool held = (padGetButtons(&pad) & chord) == chord;
  static bool was_held = false;
  static bool automatic = REXCVAR_GET(witcher2_water_capture_auto);
  if (g_capture.phase == Phase::Idle && ((held && !was_held) || automatic)) {
    automatic = false;
    g_capture.phase = Phase::Control;
    REXLOG_INFO("[water] capture armed for the next water draw");
  }
  was_held = held;
}

} // namespace renut::gpu
#else
namespace renut::gpu {
void TrackWaterCaptureTextureLocked(uint32_t, const plume::RenderTexture *, const plume::RenderTextureView *) {}
bool BeginWaterCaptureDrawLocked(uint64_t, uint64_t, const void *, size_t, const void *, size_t,
                                 const plume::RenderTexture *) { return false; }
void CaptureLightingDrawLocked(uint64_t, bool, const void *, size_t, const void *, size_t,
                               const plume::RenderTexture *) {}
void EndWaterCaptureDrawLocked(const plume::RenderTexture *) {}
void FinishWaterCaptureFrameLocked(VideoState &, const PresentImage *) {}
} // namespace renut::gpu
#endif
