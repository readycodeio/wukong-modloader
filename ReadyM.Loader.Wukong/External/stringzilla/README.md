We distribute a shared version of Stringzilla built with dynamic dispatch.
This ensures that the library can be used everywhere without throwing illegal instruction errors due to e.g. AVX2 instructions on older CPUs.

# Updating

1. `git clone https://github.com/ashvardanian/StringZilla.git --recursive`
2. `cd StringZilla`
3. `mkdir build && cd build`
4. `cmake -D STRINGZILLA_BUILD_SHARED=1 ..`
5. Open the generated solution in Visual Studio, set the configuration to Release, and build the `stringzilla_shared.vcxproj` project.
6. Update the files:
   - copy built `stringzilla.dll` to `../../../Binary`
   - copy built `stringzilla_shared.lib` to this folder
   - overwrite headers in `./include` with the ones from the source repo's `include/stringzilla` folder