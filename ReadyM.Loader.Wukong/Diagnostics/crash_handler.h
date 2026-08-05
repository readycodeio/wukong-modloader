#pragma once


// Installs the native crash reporter.
//
// Call once, as early as possible after logging is up. Produces three things when the game dies:
//
//   CSharpCrash.txt  a memory mapped text file next to CSharpLog.txt. Holds the install banner, a ring of
//                    the last exceptions seen anywhere in the process, and the full fatal report. It is
//                    written purely by memcpy into mapped pages, so the OS flushes it even if the process is
//                    killed outright or dies somewhere the exception dispatcher cannot unwind.
//   CSharpCrash.dmp  a minidump, written on a fresh thread so it also survives a stack overflow.
//   one line in      pointing at the above.
//   CSharpLog.txt
//
// Existing behaviour is preserved: the unhandled exception filter returns EXCEPTION_CONTINUE_SEARCH, so
// Unreal's crash reporter and WER still run exactly as before.
bool install_crash_handler();
