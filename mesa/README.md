# Mesa NVK on Switch

Witcher2NX uses the Horizon NVK driver from
[mesa-switch](https://github.com/danfromtico/mesa-switch). The tested library was
from a Mesa 26.2.4 unified Horizon SDK. Set `REXGLUE_SWITCH_NVK_SDK` to the
extracted `opt/devkitpro/portlibs/switch` directory, where the build expects
`lib/libvulkan.a`.

To build the dependency from source, follow the upstream unified SDK instructions
for `build-unified.sh` under MSYS2. The upstream repository also has a separate
Docker entry point for NVK. The exact driver source revision for the tested
archive was not recorded.

SHA-256 of the tested `libvulkan.a`:

```text
ff65a74b46fe749f6be73eac3c8968d70e312c7646f815f26b69f4aa35737d26
```
