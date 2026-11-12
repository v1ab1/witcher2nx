#include "cpu/sa2_enhancements.h"
#include "me/sa2_render_mode.h"
#include "renut_engine/gpu/sa2_renut_pacing.h"
#include "sa2_capture.h"

#include <switch.h>
#include <rex/ppc/context.h>
#include <rex/ppc/func.h>
#include <rex/system/gpu_plugin.h>

std::atomic<uint64_t> g_sa2_game_frames;
extern "C" const bool g_banjo_store_cards = false;

// This executable uses the D3D hooks and has no Xenos plugin factory.
extern "C" uint32_t rex_gpu_abi_version() { return 0; }
extern "C" rex::system::IGraphicsSystem* rex_gpu_create(
    uint32_t, const rex::system::GpuCreateInfo*) { return nullptr; }

REX_EXTERN(__imp__sub_82F80E40);
extern "C" REX_FUNC(sub_82F80E40) {
  // The tutorial worker constructor publishes the callback object after this
  // call. Its caller already resumes the worker once construction is complete.
  if (ctx.lr == 0x825CA2A4)
    ctx.r6.u64 = 1;
  __imp__sub_82F80E40(ctx, base);
}

extern "C" Result __real_nvGpuChannelAppendEntry(
    NvGpuChannel* channel, iova_t address, size_t words, u32 flags, u32 threshold);
extern "C" Result __wrap_nvGpuChannelAppendEntry(
    NvGpuChannel* channel, iova_t address, size_t words, u32 flags, u32 threshold) {
  // Libnx uses SubmitGpfifo below HOS 4.0, which accepts at most 0x200 entries.
  constexpr u32 legacy_threshold = GPFIFO_QUEUE_SIZE - 0x200;
  if (hosversionBefore(4, 0, 0) && threshold < legacy_threshold)
    threshold = legacy_threshold;
  return __real_nvGpuChannelAppendEntry(channel, address, words, flags, threshold);
}

namespace sa2::enh {
bool CpuMaster() { return false; }
bool NonCpuMaster() { return false; }
}

namespace sa2::capture {
bool RenutLockstepOn() { return false; }
void RenutLockstepFrame(uint64_t, const rex::ui::RawImage*) {}
}

namespace sa2::render {
void OnPresented(uint64_t) {}
}

namespace sa2::renut {
void PaceSwap(uint32_t) {}
}
