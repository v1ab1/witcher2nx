#pragma once

#include <filesystem>

namespace rex {
class Runtime;
}

bool RegisterGuestFunctions(rex::Runtime& runtime);
int RunBoot(const std::filesystem::path& game_root, const std::filesystem::path& state_root);
void ShowStatus(const char* message);
[[noreturn]] void EndBoot(int result);
