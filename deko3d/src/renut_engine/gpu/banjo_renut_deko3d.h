// banjo-nx (2026-10-17): Renut's renderer on plume's deko3d back end (renut_gpu_backend = "deko3d",
// docs/deko3d-renderer.md). The game shaders' GLSL package in the NRO (tools/deko3d_shader_package.py) and the DKSH
// cache of compiled variants on the SD card.
#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace plume {
struct RenderDevice;
struct RenderShader;
}  // namespace plume

namespace banjo::renut_gpu::deko3d {

inline constexpr uint64_t kWitcherFxaaShader = 0x14830D3955814FE1ull;

// Hot Witcher 2 shaders with a pure fragment body and the translated alpha test.
inline constexpr bool WitcherFragmentTuned(uint64_t hash) {
  switch (hash) {
    case kWitcherFxaaShader:
    case 0x021842DF8E73954Bull:
    case 0x126E21EED5144FEDull:
    case 0x12C480E60BC2B70Aull:
    case 0x172BEBF5479701AAull:
    case 0x1751BCB20327F0C0ull:
    case 0x1CE03F6D8891FBEEull:
    case 0x22DFD94B0F97BCDCull:
    case 0x29F6B9DC6D9C4061ull:
    case 0x2E44476CC5DC0CDCull:
    case 0x33382DC426620804ull:
    case 0x4C88446E985D5FE3ull:
    case 0x52048EE1B75BD711ull:
    case 0x55F0787D5079D2DEull:
    case 0x5700EDCFD7E0B634ull:
    case 0x5C2994A3FF80461Full:
    case 0x5F549466E8736C3Cull:
    case 0x5FCF0FEF678E0110ull:
    case 0x6EC8AD2DA942579Bull:
    case 0x752F459B16B77DAAull:
    case 0x755F83E354ABE2CDull:
    case 0x7B5F80AA19D1E36Cull:
    case 0x7BF017D4962F5915ull:
    case 0x8A85534A20512E9Eull:
    case 0x9AA5D944C3122354ull:
    case 0x9D074374F6638A2Aull:
    case 0xBCB105034A862A29ull:
    case 0xBEA67AEE6795A005ull:
    case 0xC922452654A99DCAull:
    case 0xE144217D0E28B058ull:
    case 0xE8EB382BEA488A47ull:
    case 0xEAB50831B93FF2CEull:
    case 0xF35919B52DA699F5ull:
    case 0xF8EED87302AB7BEEull:
      return true;
    default:
      return false;
  }
}

// Whether the NRO carries the GLSL package (Switch builds made with one).
bool PackageAvailable();

// sa2-nx (2026-10-20, 1.0.3.1): whether the GLSL packages in use draw right above 1x (made without
// tools/deko3d_glsl_opt.py's pass scale1, as the installer page makes them from 1.0.3.1 on). Read once (logged when
// not); render/surfaces.cpp keeps 1280x720 otherwise.
bool PackageScales();

// The deko3d shader (plume::Deko3DShaderBlob) of a game shader by its container hash, or null (logged once).
std::unique_ptr<plume::RenderShader> CreateGameShader(plume::RenderDevice *device, uint64_t hash);

// The boolean specialization constants (ids 1000..1255) a game shader has, as ShaderBoolSpecMask gives them.
bool BoolSpecMask(uint64_t hash, uint32_t mask[8]);

// peggle2-nx (2026-10-20): how many game shaders of the shader table have no GLSL in any package (the NRO's, or
// deko3d_glsl.bin beside it): a build without translated game shaders before the installer's file is there.
size_t MissingGameShaders();

// The DKSH cache next to the NRO (deko3d_shaders.bin): what it holds is read at start; variants the console compiles
// are added to it (written by Save, and by a periodic save while the game runs). Installed into plume before the
// device is made.
void InstallShaderCache();
void SaveShaderCache();

// One line for the renderer's report: compiles, cache hits, memory.
std::string Report();

}  // namespace banjo::renut_gpu::deko3d
