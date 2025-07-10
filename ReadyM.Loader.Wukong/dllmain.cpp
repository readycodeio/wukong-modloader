#include <iterator>
#include <windows.h>

#include "Config/debugger.h"
#include "Config/flags.h"
#include "Config/path.h"
#include "EntryDll/version_dll.h"
#include "Logger/logger.h"
#include "Mono/appdomain.h"
#include "Mono/assembly.h"
#include "Mono/debug-helpers.h"
#include "Mono/jit.h"
#include "Mono/mono-debug.h"
#include "Mono/object-internals.h"
#include "USharp/usharp.h"
#include "Windows/console.h"


static void* g_domain;
static MonoAssembly* g_assembly;
static bool g_already_init_managed;
static HMODULE g_hModule = nullptr;


constexpr const char* k_entry_point_init_method = "ReadyM.Loader.Wukong.Bootstrap.EntryPoint:Init";
constexpr const char* k_entry_point_deinit_method = "ReadyM.Loader.Wukong.Bootstrap.EntryPoint:DeInit";


static bool init_managed_mod_loader()
{
    auto image = mono_assembly_get_image(g_assembly);

    if (!image)
    {
        log_error("mono_assembly_get_image failed.");
        return false;
    }

    auto init_method_desc = mono_method_desc_new(k_entry_point_init_method, true);

    if (!init_method_desc)
    {
        log_error("Invalid method descriptor: mono_method_desc_new failed.");
        return false;
    }

    auto init_method = mono_method_desc_search_in_image(init_method_desc, image);

    if (!init_method)
    {
        log_error("Did not find the method `{}` mono_method_desc_search_in_image failed.", k_entry_point_init_method);
        mono_method_desc_free(init_method_desc);
        return false;
    }

    mono_method_desc_free(init_method_desc);

    MonoException* exc = nullptr;
    MonoObject** exc_obj = reinterpret_cast<MonoObject**>(&exc);
    mono_runtime_invoke(init_method, nullptr, nullptr, exc_obj);

    if (exc != nullptr)
    {
        log_error("mono_runtime_invoke failed with exception");
        return false;
    }

    g_already_init_managed = true;

    log_debug("CSharpLoader init success.");
    return true;
}


static void post_csharp_loader__load__callback()
{
    g_domain = mono_get_root_domain();
    if (!g_domain)
    {
        log_error("mono_get_root_domain returned null.");
        return;
    }

    log_info("Domain initialized");

    g_assembly = mono_domain_assembly_open(g_domain, "ReadyM.Loader.Wukong.Bootstrap.dll");
    if (!g_assembly)
    {
        log_error("mono_domain_assembly_open failed.");
        return;
    }

    log_info("Loaded managed mod assembly entry point.");

#ifdef LOAD_THREADED
    g_main_background_thread = CreateThread(nullptr, 0, mod_background_thread, g_hModule, 0, nullptr);
#else
    if (!init_managed_mod_loader())
    {
        log_error("init_managed_mod_loader failed.");
        return;
    }
#endif

    log_debug("post_csharp_loader__load__callback completed successfully.");
}


static void post_load_assembly_bundles()
{
    log_debug("Assembly bundles callback triggered.");

    auto mod_dir = get_mod_dir();
    auto dirs = std::vector
    {
        mod_dir / L"Overrides",
        std::filesystem::path(L"CSharpLoader") / "ReadyM.Loader.Wukong.Bootstrap.dll"
    };

    if (load_assembly_bundles(dirs))
    {
        log_info("Loaded assembly bundle overrides.");
    }
    else
    {
        log_error("Failed to load assembly bundle overrides.");
    }
}


static std::wstring get_version_string()
{
    auto dll_path = get_base_dir() / L"CSharpLoader" / L"ReadyM.Loader.Wukong.Managed.dll";

    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(dll_path.c_str(), &handle);

    std::vector<BYTE> version_data(size);
    VS_FIXEDFILEINFO* file_info = nullptr;
    UINT len = 0;

    if (size == 0)
        goto unknown;

    if (!GetFileVersionInfoW(dll_path.c_str(), handle, size, version_data.data()))
        goto unknown;

    if (!VerQueryValueW(version_data.data(), L"\\", reinterpret_cast<LPVOID*>(&file_info), &len))
        goto unknown;

    if (file_info)
    {
        auto major = HIWORD(file_info->dwFileVersionMS);
        auto minor = LOWORD(file_info->dwFileVersionMS);
        auto build = HIWORD(file_info->dwFileVersionLS);
        auto revision = LOWORD(file_info->dwFileVersionLS);
        return std::format(L"{}.{}.{}.{}", major, minor, build, revision);
    }

unknown:
    log_error(L"Mod loader version could not be determined: {}", dll_path.wstring());
    return L"Unknown Version";
}


static std::wstring get_title_string()
{
    return LR"(
 ____                _       __  __   _                    _           
|  _ \ ___  __ _  __| |_   _|  \/  | | |    ___   __ _  __| | ___ _ __ 
| |_) / _ \/ _` |/ _` | | | | |\/| | | |   / _ \ / _` |/ _` |/ _ \ '__|
|  _ <  __/ (_| | (_| | |_| | |  | | | |__| (_) | (_| | (_| |  __/ |   
|_| \_\___|\__,_|\__,_|\__, |_|  |_| |_____\___/ \__,_|\__,_|\___|_|   
                       |___/                                          
)";
}


static std::wstring get_cmdline()
{
    auto cmdline = GetCommandLineW();
    return cmdline;
}

static std::wstring get_environment_variable(const std::wstring& variable)
{
    const DWORD initialSize = 512;
    std::wstring buffer(initialSize, L'\0');

    DWORD size = GetEnvironmentVariableW(variable.c_str(), &buffer[0], static_cast<DWORD>(buffer.size()));

    if (size == 0)
    {
        const DWORD error = GetLastError();
        if (error == ERROR_ENVVAR_NOT_FOUND || (error == 0 && buffer.empty()))
            return L"";

        log_error(L"GetEnvironmentVariableW failed for '{}': {}", variable, error);
        return L"";
    }

    if (size > buffer.size())
    {
        buffer.resize(size);
        size = GetEnvironmentVariableW(variable.c_str(), &buffer[0], size);
        if (size == 0)
        {
            const DWORD error = GetLastError();
            log_error(L"GetEnvironmentVariableW failed for '{}': {}", variable, error);
            return L"";
        }
    }

    buffer.resize(size); // trim to actual length
    return buffer;
}

static std::vector<std::wstring> parse_cmdline(std::wstring cmdline)
{
    int argc;
    auto argv = CommandLineToArgvW(cmdline.c_str(), &argc);

    std::vector<std::wstring> result;
    if (argv)
    {
        for (auto i = 0; i < argc; ++i)
        {
            result.push_back(argv[i]);
        }
        LocalFree(argv);
    }

    return result;
}


static bool init_embed_runtime()
{
    auto enable_console_flag = load_enable_console();

    if (enable_console_flag == 1)
    {
        create_console();
        log_info(L"ReadyM WukongMp C# Loader ver. {} {}", get_version_string(), get_title_string());
    }

    auto mod_dir_override = get_environment_variable(L"WUKONGMP_MOD_FOLDER");
    if (!mod_dir_override.empty())
    {
        set_mod_dir_override(mod_dir_override);
        log_debug(L"Mod folder override: {}", get_mod_dir().c_str());
    }
    else
    {
        log_debug(L"Mod folder: {}", get_mod_dir().c_str());
    }

    auto enable_jit_flag = load_enable_jit();
    auto enable_develop_flag = load_enable_develop();

    log_info("Intercepting USharp init.");
    if (!intercept_csharp_loader__load(&post_csharp_loader__load__callback))
    {
        log_error("Failed to intercept USharp init.");
        return false;
    }

    log_info("Intercepting register bundled.");
    if (!intercept_register_bundled_assemblies(&post_load_assembly_bundles))
    {
        log_error("Failed to intercept register bundled assemblies.");
    }

    auto mod_dir = get_mod_dir();

    log_info("CSharpLoader EnableDevelop flag: {}", enable_develop_flag);
    if (enable_develop_flag)
    {
        if (!load_debugger_symbols(mod_dir / L"ReflectionOnly"))
        {
            log_error("Failed to load debugger symbols.");
            // NOTE: non-fatal error
        }

        auto log_level = load_log_level();
        auto log_mask = load_log_mask();
        auto debugger_agent_opts = load_debugger_agent_opts();
        if (!init_debugger(log_level, log_mask, debugger_agent_opts))
        {
            log_error("Failed to initialize debugger.");
            // NOTE: non-fatal error
        }
    }

    log_info("CSharpLoader EnableJit flag: {}", enable_jit_flag);
    if (enable_jit_flag)
    {
        if (!enable_jit())
        {
            log_error("Failed to enable JIT.");
            // NOTE: non-fatal error
        }
    }

    return true;
}


static void init_dll(HMODULE hModule)
{
    DisableThreadLibraryCalls(hModule);
    g_hModule = hModule;

    init_version_dll();
    init_embed_runtime();
}


static void deinit_dll()
{
    deinit_version_dll();
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        init_dll(hModule);
        break;
    case DLL_PROCESS_DETACH:
        deinit_dll();
        break;
    default:
        // no-op
        break;
    }

    return TRUE;
}
