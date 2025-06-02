#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include "Utils/string.h"

#include <codecvt>
#include <locale>
#include <string>

std::string trim(std::string str)
{
    size_t endpos = str.find_last_not_of(" \t\r\n");
    size_t startpos = str.find_first_not_of(" \t\r\n");
    if( std::wstring::npos != endpos )
    {
        str = str.substr( 0, endpos+1 );
        str = str.substr( startpos );
    }
    else
    {
        str.erase(std::remove(std::begin(str), std::end(str), ' '), std::end(str));
    }

    return str;
}

std::wstring utf8_to_wstring(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.from_bytes(str);
}

std::string wstring_to_utf8(const std::wstring& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.to_bytes(str);
}