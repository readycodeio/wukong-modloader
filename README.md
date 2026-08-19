# WukongMP Mod Loader

The mod loader behind [WukongMP](https://ready.mp), the multiplayer mod for Black Myth: Wukong. It injects into the game
as a proxy DLL, starts a Mono runtime host, and loads managed mods from the game's `Mods` folder.

Mods themselves are built against the [WukongMP SDK](https://github.com/readycodeio/wukong-csharp-mod), not against this
repository. Start there if you want to write a mod.

## Layout

|                                    |                                                                                                                                     |
|------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------|
| `ReadyM.Loader.Wukong`             | Native proxy DLL, built as `dxgi.dll` and dropped next to the game executable                                                       |
| `ReadyM.Loader.Wukong.Bootstrap`   | First-stage managed entry point: assembly resolution and DI setup                                                                   |
| `ReadyM.Loader.Wukong.Managed`     | Managed loader: patching, debugger support, mod discovery                                                                           |
| `ReadyM.Loader.Executable`         | Host process used to launch the game with the loader in place                                                                       |
| `CSharpModBase`, `CSharpModBaseV2` | The interfaces a mod implements. Referenced by the SDK                                                                              |
| `HarmonyPrepatcher`                | Submodule: PreludeLib, the prepatcher built on Harmony                                                                              |
| `Binary/version.dll`               | Compatibility shim for installs that already had [B1CSharpLoader](https://github.com/czastack/B1CSharpLoader) by czastack installed |
| `Config/`                          | Runtime configuration shipped with the loader                                                                                       |

## Building

Requirements:

* [.NET 10.0 SDK](https://dotnet.microsoft.com/en-us/download/dotnet/10.0) or later
* Visual Studio 2022 or later with the C++ toolset (`v145`), for the native project
* Git LFS, since some binaries are stored through it

```
git clone --recursive https://github.com/readycodeio/wukong-modloader.git
cd wukong-modloader
dotnet build EmbedCSharpLoader.sln -c Release
```

The `--recursive` matters: `HarmonyPrepatcher` and its own `Harmony` submodule are both required to build.

The managed projects compile against the game's assemblies through the
[`ReadyM.Wukong.GameRefs`](https://github.com/readycodeio/wukong-game-refs) NuGet package. Those are reference-only
assemblies, API surface with no method bodies, resolved at compile time only. No game files are needed to build, and
none are distributed here.

`CopyToGameFolder.ps1` locates the game through Steam and copies a built loader into place.

## Configuration

`Config/b1cs.ini` controls the loader at runtime: console visibility, JIT, game assembly patching, and a few diagnostic
switches documented inline. `Config/debugger-agent.txt`
configures the Mono debugger agent, which listens on `127.0.0.1:44446` by default.

## Licensing

This repository has no licence yet, so no rights are granted for reuse.

Third-party code carries its own terms:

* `ReadyM.Loader.Wukong/External/stringzilla` is [StringZilla](https://github.com/ashvardanian/StringZilla)
  by Ash Vardanian, Apache-2.0, licence included in that directory.
* `HarmonyPrepatcher` is MIT, and its `Harmony` submodule is
  [Harmony](https://github.com/pardeike/Harmony) by Andreas Pardeike, also MIT.
* `Binary/version.dll` derives from [B1CSharpLoader](https://github.com/czastack/B1CSharpLoader)
  by czastack, with our changes. Upstream published no licence.

Game assemblies are the property of Game Science and are not included in this repository.
