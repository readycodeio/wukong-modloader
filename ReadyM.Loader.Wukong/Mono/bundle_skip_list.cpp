#include "bundle_skip_list.h"

#include <cctype>

#include "Config/flags.h"


namespace
{
    bool equal_ignoring_case(std::string_view a, std::string_view b)
    {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        }

        return true;
    }
}


bool skip_entry_matches(std::string_view entry, std::string_view assembly_name)
{
    constexpr std::string_view whitespace = " \t\r\n";

    const auto first = entry.find_first_not_of(whitespace);
    if (first == std::string_view::npos)
        return false;

    entry = entry.substr(first, entry.find_last_not_of(whitespace) - first + 1);

    if (equal_ignoring_case(entry, assembly_name))
        return true;

    // Allow the entry to omit the extension, since that is how people write assembly names.
    constexpr std::string_view dll = ".dll";
    if (assembly_name.size() > dll.size() &&
        equal_ignoring_case(assembly_name.substr(assembly_name.size() - dll.size()), dll))
    {
        return equal_ignoring_case(entry, assembly_name.substr(0, assembly_name.size() - dll.size()));
    }

    return false;
}


bool is_replacement_skipped(const std::string& assembly_name)
{
    static const std::string s_list = load_skip_bundle_replacement();

    if (s_list.empty())
        return false;

    size_t start = 0;
    while (start <= s_list.size())
    {
        auto end = s_list.find_first_of(",;", start);
        if (end == std::string::npos)
            end = s_list.size();

        if (skip_entry_matches(std::string_view(s_list).substr(start, end - start), assembly_name))
            return true;

        start = end + 1;
    }

    return false;
}
