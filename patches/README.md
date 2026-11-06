# Patches

- `generated-witcher2.patch` is applied by `tools/generate.py` to the ReXGlue
  0.10.0 output for the verified executable. It adds GCC weak aliases, handles
  byte-reversed 64-bit loads and unresolved tail branches, and restores the
  graphics checkpoint, loading queue hook, worker retry and audio error log.
- `sa2-nx-baf523b.patch` records the changes from the SA2-NX snapshot to this
  project's SDK, including the Xenos/Vulkan and runtime fixes. The changes are
  already applied in `sdk/`.
- `rexglue-sdk-v0.10.0.patch` and `ffmpeg-0604b46.patch` are the inherited
  SA2-NX provenance patches for the SDK and FFmpeg overlay. The snapshot
  already includes their changes.
