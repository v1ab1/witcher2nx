#include "boot.h"
#include "witcher2_init.h"

#include <rex/rex_app.h>

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
witcher2_vulkan_compatibility = true
async_shader_compilation = false
vulkan_dynamic_rendering = false
gpu_allow_invalid_fetch_constants = true
)"
#endif
        R"(
log_level = "debug"
log_to_file = true
asset_pack = false
asset_pack_build = false
allow_game_relative_writes = false
execute_unclipped_draw_vs_on_cpu = true
execute_unclipped_draw_vs_on_cpu_with_scissor = true
)";
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    config.gpu_plugin = "xenos";
  }

  bool ConstructRuntime(const rex::PathConfig& paths) override {
    return rex::ReXApp::ConstructRuntime(paths) && RegisterGuestFunctions(*runtime());
  }
};

}  // namespace

REX_DEFINE_APP(witcher2, Witcher2App::Create)
