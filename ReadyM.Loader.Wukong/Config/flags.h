#pragma once
#include <string>

int load_enable_console();
int load_enable_jit();
int load_enable_develop();

// Diagnostic switch, read by the managed bootstrap too. Logged here so the mode is visible at the top of
// the log next to the other flags, rather than only where it takes effect.
int load_patch_game_assemblies();

// Comma separated list of bundled assemblies to leave exactly as the game shipped them, instead of pointing
// Mono at our Overrides copy. Names may be given with or without the .dll suffix and are matched
// case insensitively. Empty by default, meaning every override is applied as normal.
//
//     SkipBundleReplacement=LiteNetLib,System.Memory
//
// Diagnostic only: the mod is compiled against the override versions, so leaving one out will usually produce
// a managed TypeLoad or MissingMethod exception. That is still a useful outcome, because a clean managed
// exception where there used to be a native crash tells us the replacement is what the crash depends on.
std::string load_skip_bundle_replacement();

// 1 (default) lets the vectored handler turn a first chance hard fault into a full report and a minidump.
// 0 leaves the exception ring recording, which is passive, but writes nothing until the unhandled filter runs.
//
// This exists to rule out our own reporting as the cause of a death. Writing a report calls into dbghelp and
// MiniDumpWriteDump, from inside a first chance handler, on a thread that has just faulted inside the game's
// obfuscated code. If that fault is one the game's protection normally handles itself, our reporting could be
// what actually stops the process, in which case every "FIRST CHANCE" report we have collected is describing a
// survivable exception that we then made fatal.
int load_crash_handler_escalate();
