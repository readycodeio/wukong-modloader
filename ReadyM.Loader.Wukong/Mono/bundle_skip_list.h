#pragma once
#include <string>
#include <string_view>


// Matches one entry of the SkipBundleReplacement list against a bundle name, ignoring case, surrounding
// whitespace, and a missing .dll suffix, so "litenetlib" and " LiteNetLib.dll " both match "LiteNetLib.dll".
// Exposed rather than kept private so it can be tested against the exact names the loader compares.
bool skip_entry_matches(std::string_view entry, std::string_view assembly_name);


// True when b1cs.ini's SkipBundleReplacement names this bundle, meaning the game's own copy should be left in
// place and our override ignored. The list is read once on first use.
bool is_replacement_skipped(const std::string& assembly_name);
