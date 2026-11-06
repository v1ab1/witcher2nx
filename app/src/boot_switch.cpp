#include "boot.h"

#include <cstdio>
#include <cstdlib>
#include <exception>

#include <switch.h>

extern "C" {
extern u32 __nx_applet_exit_mode;
NX_NORETURN void __libnx_exit(int rc);
void RexSwitchRedirectStderr(void);
}

void ShowStatus(const char* message) {
  std::printf("%s\n", message);
  consoleUpdate(nullptr);
}

[[noreturn]] void EndBoot(int result) {
  std::fflush(nullptr);
  __nx_applet_exit_mode = 1;
  __libnx_exit(result);
}

int main() {
  consoleInit(nullptr);
  ShowStatus("The Witcher 2 - boot test");
  RexSwitchRedirectStderr();
  constexpr const char* root = "/switch/witcher2";
  int result = EXIT_FAILURE;
  try {
    result = RunBoot(std::filesystem::path(root) / "game_root",
                     std::filesystem::path(root) / "logs");
  } catch (const std::exception& error) {
    std::fprintf(stderr, "Boot error: %s\n", error.what());
    ShowStatus(error.what());
  }
  ShowStatus("Press + to close.");
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  PadState pad;
  padInitializeDefault(&pad);
  while (appletMainLoop()) {
    padUpdate(&pad);
    if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
      break;
    }
    consoleUpdate(nullptr);
  }
  consoleExit(nullptr);
  EndBoot(result);
}
