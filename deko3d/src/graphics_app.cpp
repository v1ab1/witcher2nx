#include "boot.h"
#include "witcher2_init.h"

#include <rex/cvar.h>
#include <rex/rex_app.h>
#include <rex/logging.h>

#include "renut_engine/gpu/overlay/app_integration.h"

#if REX_PLATFORM_SWITCH
REXCVAR_DEFINE_BOOL(switch_profile_stacks, false, "Witcher 2/Diagnostics",
                    "Sample thread stacks in logs/rex/rex_profile.log.")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
#endif

namespace {

class Witcher2App : public rex::ReXApp {
 public:
  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& context) {
    return std::unique_ptr<Witcher2App>(new Witcher2App(context, "witcher2", PPCImageConfig));
  }

 private:
  using rex::ReXApp::ReXApp;

#if REX_PLATFORM_SWITCH
  void OnConfigurePaths(rex::PathConfig& paths) override {
    const std::filesystem::path root = "/switch/witcher2";
    paths.game_data_root = root / "game_root";
    paths.user_data_root = root / "user";
    paths.cache_root = paths.user_data_root / "cache";
    paths.config_path = root / "witcher2.toml";
    std::filesystem::create_directories(root / "logs");
  }
#endif

  std::string_view BuiltInConfig() const override {
    return
#if REX_PLATFORM_SWITCH
        R"(log_file = "/switch/witcher2/logs/graphics.log"
log_level = "info"
witcher2_vulkan_compatibility = true
witcher2_skip_overwritten_depth_transfers = true
async_shader_compilation = false
vulkan_dynamic_rendering = false
gpu_allow_invalid_fetch_constants = true
)"
#else
        R"(log_level = "debug"
)"
#endif
        R"(log_to_file = true
asset_pack = false
asset_pack_build = false
allow_game_relative_writes = false
execute_unclipped_draw_vs_on_cpu = true
execute_unclipped_draw_vs_on_cpu_with_scissor = true
)";
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    renut::gpu::ConfigureRuntime(config);
    REXLOG_INFO("Witcher 2 graphics: Direct3D to deko3d");
  }

  std::unique_ptr<rex::ui::ImmediateDrawer> OnCreateImmediateDrawer() override {
    return renut::gpu::CreateOverlayDrawer();
  }

  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& paths, std::function<void(rex::PathConfig)> resume) override {
    if (!renut::gpu::StartRenderer(app_context(), [this] { return imgui_drawer(); })) {
      throw std::runtime_error("deko3d renderer initialization failed");
    }
    return rex::ReXApp::OnFinalizePaths(paths, std::move(resume));
  }

  void OnShutdown() override { renut::gpu::ShutdownRenderer(); }

  bool ConstructRuntime(const rex::PathConfig& paths) override {
    return rex::ReXApp::ConstructRuntime(paths) && RegisterGuestFunctions(*runtime());
  }
};

}  // namespace

REX_DEFINE_APP(witcher2, Witcher2App::Create)
