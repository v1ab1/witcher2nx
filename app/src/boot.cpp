#include "boot.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include <rex/audio/nop/nop_audio_system.h>
#include <rex/cvar.h>
#include <rex/input/input_system.h>
#include <rex/kernel/init.h>
#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>
#include <rex/system/user_module.h>
#include <rex/system/xex_module.h>
#include <rex/system/xthread.h>

#include "witcher2_init.h"

#if defined(WITCHER2_RENDERER)
REX_EXTERN(__imp__VdInitializeRingBuffer);
#endif

namespace {

PPCFunc* entry_function = nullptr;
#if !defined(WITCHER2_RENDERER)
std::atomic<bool> graphics_checkpoint{false};
constexpr rex::X_STATUS kSuccess = 0;
#endif

void GuestEntry(PPCContext& context, uint8_t* base) {
  REXLOG_INFO("Entering recompiled Witcher 2 code at 0x82880808");
  rex::FlushLogging();
  entry_function(context, base);
  REXLOG_INFO("Returned from recompiled Witcher 2 entry point");
  rex::FlushLogging();
}

#include "recovered_vtables.inc"

// sub_827D1740 builds this callback table at runtime. These seven entries
// were missing from the static recompiler's function map.
void ForwardCallback827D1570(PPCContext& ctx, uint8_t* base) {
  ctx.r3.u64 = REX_LOAD_U32(ctx.r4.u32);
  ctx.r11.u64 = ctx.r5.u64;
  ctx.r5.u64 = ctx.r6.u64;
  ctx.r6.u64 = ctx.r7.u64;
  ctx.r7.u64 = ctx.r9.u64;
  ctx.r4.u64 = ctx.r11.u64;
  ctx.r10.u64 = REX_LOAD_U32(ctx.r3.u32);
  ctx.r9.u64 = REX_LOAD_U32(ctx.r10.u32 + 0x20);
  ctx.ctr.u64 = ctx.r9.u64;
  REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
}

void ForwardCallback827D1598(PPCContext& ctx, uint8_t* base) {
  ctx.r10.u64 = REX_LOAD_U32(ctx.r6.u32);
  ctx.r11.u64 = ctx.r6.u64;
  ctx.r9.s64 = -0x7df20000;
  ctx.r5.u64 = REX_LOAD_U32(ctx.r4.u32);
  ctx.r6.u64 = 0;
  ctx.r4.s64 = ctx.r9.s64 + 0x1dc8;
  ctx.r3.u64 = ctx.r11.u64;
  ctx.r8.u64 = REX_LOAD_U32(ctx.r10.u32 + 0x0c);
  ctx.ctr.u64 = ctx.r8.u64;
  REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
}

template <uint32_t MethodOffset>
void ForwardCallbackFromR5(PPCContext& ctx, uint8_t* base) {
  ctx.r3.u64 = REX_LOAD_U32(ctx.r4.u32);
  ctx.r4.u64 = ctx.r5.u64;
  ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32);
  ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + MethodOffset);
  ctx.ctr.u64 = ctx.r10.u64;
  REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
}

void ForwardCallback827D15D8(PPCContext& ctx, uint8_t* base) {
  ctx.r3.u64 = REX_LOAD_U32(ctx.r4.u32);
  ctx.r6.u64 = ctx.r7.u64;
  ctx.r11.u64 = REX_LOAD_U32(ctx.r3.u32);
  ctx.r10.u64 = REX_LOAD_U32(ctx.r11.u32 + 0x28);
  ctx.ctr.u64 = ctx.r10.u64;
  REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
}

constexpr PPCFuncMapping kRecoveredCallbacks[] = {
    {0x827D1570, ForwardCallback827D1570},
    {0x827D1598, ForwardCallback827D1598},
    {0x827D15C0, ForwardCallbackFromR5<0x24>},
    {0x827D15D8, ForwardCallback827D15D8},
    {0x827D15F0, ForwardCallbackFromR5<0x2c>},
    {0x827D1608, ForwardCallbackFromR5<0x30>},
    {0x827D1620, ForwardCallbackFromR5<0x34>},
};

// The script dispatcher calls this through its opcode table at 0x82F68530.
void ReadScriptOperand32(PPCContext& ctx, uint8_t* base) {
  ctx.r11.u64 = REX_LOAD_U32(ctx.r4.u32 + 24);
  ctx.cr6.compare<uint32_t>(ctx.r5.u32, 0, ctx.xer);
  ctx.r10.s64 = ctx.r11.s64 + 4;
  REX_STORE_U32(ctx.r4.u32 + 24, ctx.r10.u32);
  if (!ctx.cr6.eq) {
    ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32);
    REX_STORE_U32(ctx.r5.u32, ctx.r11.u32);
  }
}

// Movie decoder callback at 0x833C0D68. Fill the block pointers, then
// dispatch through the game's transform table with the original registers.
void PrepareMovieBlock(PPCContext& ctx, uint8_t* base) {
  const uint32_t decoder = ctx.r3.u32;
  ctx.r4.u64 = ctx.r3.u64 + 0x1220;
  ctx.r8.u64 = REX_LOAD_U32(decoder + 0x1458) << 3;
  ctx.r10.u64 = REX_LOAD_U32(decoder + 0x145c) << 3;
  ctx.r7.s64 = static_cast<int16_t>(REX_LOAD_U16(decoder + 0x13bc));
  ctx.r11.s64 = static_cast<int32_t>(ctx.r7.u32 * ctx.r8.u32);
  ctx.r11.u64 += ctx.r10.u64;
  ctx.r7.s64 = static_cast<int16_t>(REX_LOAD_U16(decoder + 0x13be));
  ctx.r6.u64 = REX_LOAD_U32(decoder + 0x13b0) + ctx.r11.u64;
  REX_STORE_U32(decoder + 0x1224, ctx.r6.u32);
  ctx.r9.s64 = static_cast<int32_t>((ctx.r8.u32 << 1) * ctx.r7.u32);
  ctx.r10.u64 = (ctx.r10.u32 << 1) + ctx.r9.u64;
  ctx.r8.u64 = REX_LOAD_U32(decoder + 0x13b4) + ctx.r11.u64;
  REX_STORE_U32(decoder + 0x122c, ctx.r8.u32);
  ctx.r7.u64 = ctx.r7.u32 << 3;
  ctx.r11.u64 = REX_LOAD_U32(decoder + 0x13b8) + ctx.r10.u64;
  ctx.xer.ca = ctx.r10.s32 < 0 && (ctx.r10.u32 & 0xF) != 0;
  ctx.r6.s64 = ctx.r10.s32 >> 4;
  REX_STORE_U32(decoder + 0x1234, ctx.r11.u32);
  REX_STORE_U32(decoder + 0x123c, ctx.r11.u32 + 8);
  ctx.r11.u64 = ctx.r7.u64 + ctx.r11.u32;
  ctx.r9.s64 = static_cast<int32_t>(0x83549CF0u);
  ctx.r10.u64 = (ctx.r6.u32 << 3) & 0x78;
  REX_STORE_U32(decoder + 0x1244, ctx.r11.u32);
  ctx.r8.u64 = ctx.r11.u64 + 8;
  REX_STORE_U32(decoder + 0x124c, ctx.r8.u32);
  ctx.r5.u64 = REX_LOAD_U32(decoder + 0x1140);
  ctx.r6.u64 = REX_LOAD_U32(ctx.r9.u32 + ctx.r10.u32 + 4);
  ctx.r7.u64 = REX_LOAD_U32(ctx.r9.u32 + ctx.r10.u32);
  ctx.ctr.u64 = ctx.r7.u64;
  REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
}

#if !defined(WITCHER2_RENDERER)
int Fail(const char* message, unsigned status) {
  REXLOG_ERROR("{} (status 0x{:08X})", message, status);
  rex::FlushLogging();
  ShowStatus(message);
  return EXIT_FAILURE;
}
#endif

}  // namespace

bool Witcher2ShouldDrainLoadingQueue(PPCContext& ctx, uint8_t* base) {
#if defined(WITCHER2_RENDERER)
  // The render worker normally processes one command between loading-screen
  // updates. A level can queue thousands of commands before its completion
  // semaphore, so give the queue time to drain between those updates.
  constexpr uint32_t kLoadingQueueBacklog = 32;
  const uint32_t worker = ctx.r26.u32;
  const uint32_t pending = REX_LOAD_U32(worker + 20);
  static thread_local bool just_updated = false;
  static thread_local std::chrono::steady_clock::time_point next_update;
  const bool loading_screen_active =
      REX_LOAD_U32(worker + 624) || REX_LOAD_U32(worker + 628);
  if (!loading_screen_active || pending < kLoadingQueueBacklog) {
    just_updated = false;
    next_update = {};
    // When ordinary work is ready outside loading, render the queued scene
    // instead of generating an extra frame from an empty draw list.
    return !loading_screen_active;
  }
  const auto now = std::chrono::steady_clock::now();
  // Start the drain window on re-entry, after the previous update has returned.
  if (just_updated) {
    next_update = now + std::chrono::milliseconds(100);
    just_updated = false;
  }
  if (now < next_update) {
    return true;
  }
  just_updated = true;
  return false;
#else
  return false;
#endif
}

bool RegisterGuestFunctions(rex::Runtime& runtime) {
  const auto module = runtime.kernel_state()->GetExecutableModule();
  const auto* xex = module ? module->xex_module() : nullptr;
  const auto* info = xex ? xex->opt_execution_info() : nullptr;
  if (!info || module->entry_point() != 0x82880808 ||
      info->media_id != 0x414AAB82 || info->title_id != 0x4E4D0855) {
    REXLOG_ERROR("The executable does not match the disc 1 build.");
    return false;
  }
  auto* dispatcher = runtime.function_dispatcher();
  entry_function = dispatcher->GetFunction(module->entry_point());
  if (!entry_function || !dispatcher->SetFunction(module->entry_point(), GuestEntry)) {
    REXLOG_ERROR("No recompiled entry point is registered.");
    return false;
  }
  for (const auto& mapping : kRecoveredVtableMethods) {
    if (!dispatcher->SetFunction(static_cast<uint32_t>(mapping.guest), mapping.host)) {
      REXLOG_ERROR("Could not register recovered vtable method at 0x{:08X}.", mapping.guest);
      return false;
    }
  }
  for (const auto& mapping : kRecoveredCallbacks) {
    if (!dispatcher->SetFunction(static_cast<uint32_t>(mapping.guest), mapping.host)) {
      REXLOG_ERROR("Could not register recovered callback at 0x{:08X}.", mapping.guest);
      return false;
    }
  }
  // FMOD builds this file-close callback on the stack. Its only instruction
  // tail-branches to the already recompiled implementation.
  return dispatcher->SetFunction(0x828BEEF0, sub_828A24E8) &&
         dispatcher->SetFunction(0x82F68530, ReadScriptOperand32) &&
         dispatcher->SetFunction(0x833C0D68, PrepareMovieBlock);
}

void Witcher2GraphicsCheckpoint(PPCContext& ctx, uint8_t* base) {
  REXLOG_INFO("BOOT CHECKPOINT: game reached GPU ring buffer setup (address=0x{:08X}, size_log2={})",
              ctx.r3.u32, ctx.r4.u32);
  rex::FlushLogging();
#if defined(WITCHER2_RENDERER)
  __imp__VdInitializeRingBuffer(ctx, base);
#else
  graphics_checkpoint.store(true, std::memory_order_release);
  rex::system::XThread::GetCurrentThread()->Exit(0);
  std::abort();
#endif
}

#if !defined(WITCHER2_RENDERER)
int RunBoot(const std::filesystem::path& game_root, const std::filesystem::path& state_root) {
  rex::InitLoggingEarly();
  std::error_code error;
  std::filesystem::create_directories(state_root, error);
  if (error) {
    ShowStatus("Could not create the diagnostic log directory.");
    return EXIT_FAILURE;
  }

  const auto log_file = (state_root / "boot.log").string();
  rex::LogConfig logging;
  logging.log_file = log_file.c_str();
  logging.default_level = spdlog::level::debug;
  logging.flush_level = spdlog::level::info;
#if !defined(__SWITCH__)
  logging.log_to_console = true;
#endif
  rex::InitLogging(logging);

  rex::cvar::SetFlagByName("asset_pack_build", "false");
  rex::cvar::SetFlagByName("asset_pack", "false");
  rex::cvar::SetFlagByName("allow_game_relative_writes", "false");

  REXLOG_INFO("Witcher 2 boot test 0.1.2, Xbox 360 disc 1, renderer disabled");
  REXLOG_INFO("Game folder: {}", game_root.string());
  if (!std::filesystem::is_regular_file(game_root / "default.xex", error)) {
    return Fail("Missing game_root/default.xex.", 0);
  }

  rex::Runtime runtime(game_root, state_root / "user");
  rex::RuntimeConfig config;
  config.kernel_init = rex::kernel::InitializeKernel;
  config.audio_factory = REX_AUDIO_BACKEND(rex::audio::nop::NopAudioSystem);
  config.input_factory = rex::input::CreateDefaultInputSystem;
  config.tool_mode = true;
  ShowStatus("Preparing the Xbox 360 runtime...");
  auto status = runtime.Setup(PPCImageConfig, std::move(config));
  if (status != kSuccess) {
    return Fail("Could not initialize the Xbox 360 runtime.", status);
  }
  status = runtime.LoadXexImage("game:\\default.xex");
  if (status != kSuccess) {
    return Fail("Could not load the game executable.", status);
  }

  if (!RegisterGuestFunctions(runtime)) {
    return Fail("Could not register the recompiled game executable.", 0);
  }

  ShowStatus("Launching the recompiled game. See logs/boot.log.");
  auto thread = runtime.LaunchModule();
  if (!thread) {
    return Fail("Could not launch the Xbox 360 game thread.", 0);
  }
  uint64_t relative_timeout = static_cast<uint64_t>(-30 * 10'000'000LL);
  status = thread->Wait(0, 0, 0, &relative_timeout);
  REXLOG_INFO("Game thread wait finished with status 0x{:08X}", status);
  rex::FlushLogging();
  const bool passed = status == kSuccess && graphics_checkpoint.load(std::memory_order_acquire);
  ShowStatus(passed ? "Boot test passed. Graphics checkpoint reached." :
                      "Boot test stopped early. See logs/boot.log.");
  // Exit the process before destroying a runtime that may have active game threads.
  EndBoot(passed ? EXIT_SUCCESS : EXIT_FAILURE);
}

#if !defined(__SWITCH__)
void ShowStatus(const char* message) {
  std::fprintf(stderr, "%s\n", message);
}

[[noreturn]] void EndBoot(int result) {
  std::fflush(nullptr);
  std::_Exit(result);
}

int main(int argc, char** argv) {
  if (argc != 3) {
    ShowStatus("Usage: witcher2 <disc 1 folder> <diagnostic output folder>");
    return EXIT_FAILURE;
  }
  return RunBoot(argv[1], argv[2]);
}
#endif
#endif
