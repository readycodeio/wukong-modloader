#include <codecvt>
#include <unordered_map>
#include <fstream>
#include <iterator>
#include <shlobj.h>
#include <shobjidl.h>
#include <combaseapi.h>
#include <psapi.h>
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
#include "Mono/mono-error.h"
#include "Mono/object-internals.h"
#include "Unreal/signature_check.h"
#include "USharp/usharp.h"
#include "Utils/deferred_call.h"
#include "Windows/console.h"


static void* g_domain;
static MonoAssembly* g_assembly;
static bool g_already_init_managed;
static HMODULE g_hModule = nullptr;


constexpr const char* k_entry_point_init_logging_method = "ReadyM.Loader.Wukong.Bootstrap.EntryPoint:InitLogging";
constexpr const char* k_entry_point_preprocess_method = "ReadyM.Loader.Wukong.Bootstrap.EntryPoint:Preprocess";
constexpr const char* k_entry_point_init_method = "ReadyM.Loader.Wukong.Bootstrap.EntryPoint:Init";
constexpr const char* k_entry_point_late_init_method = "ReadyM.Loader.Wukong.Bootstrap.EntryPoint:LateInit";
constexpr const char* k_entry_point_deinit_method = "ReadyM.Loader.Wukong.Bootstrap.EntryPoint:DeInit";


static bool bootstrap_init()
{
    auto image = mono_assembly_get_image(g_assembly);

    if (!image)
    {
        log_error("mono_assembly_get_image failed");
        return false;
    }

    auto init_logging_method_desc = mono_method_desc_new(k_entry_point_init_logging_method, true);
    defer([&] { mono_method_desc_free(init_logging_method_desc); });

    if (!init_logging_method_desc)
    {
        log_error("Invalid method descriptor: mono_method_desc_new failed for {}", k_entry_point_init_logging_method);
        return false;
    }

    auto init_logging_method = mono_method_desc_search_in_image(init_logging_method_desc, image);
    if (!init_logging_method)
    {
        log_error("Did not find the method `{}` mono_method_desc_search_in_image failed", k_entry_point_init_logging_method);
        return false;
    }
    
    auto preprocess_method_desc = mono_method_desc_new(k_entry_point_preprocess_method, true);
    defer([&] { mono_method_desc_free(preprocess_method_desc); });
    
    if (!preprocess_method_desc)
    {
        log_error("Invalid method descriptor: mono_method_desc_new failed for {}", k_entry_point_preprocess_method);
        return false;
    }

    auto preprocess_method = mono_method_desc_search_in_image(preprocess_method_desc, image);
    if (!preprocess_method)
    {
        log_error("Did not find the method `{}` mono_method_desc_search_in_image failed", k_entry_point_preprocess_method);
        return false;
    }
    
    auto init_method_desc = mono_method_desc_new(k_entry_point_init_method, true);
    defer([&] { mono_method_desc_free(init_method_desc); });

    if (!init_method_desc)
    {
        log_error("Invalid method descriptor: mono_method_desc_new failed for {}", k_entry_point_init_method);
        return false;
    }

    auto init_method = mono_method_desc_search_in_image(init_method_desc, image);
    if (!init_method)
    {
        log_error("Did not find the method `{}` mono_method_desc_search_in_image failed", k_entry_point_init_method);
        return false;
    }

    MonoException* exc = nullptr;
    MonoObject** exc_obj = reinterpret_cast<MonoObject**>(&exc);

    void* init_logging_params[2] = {
        &g_log_file_handle,
        nullptr
    };
    
    mono_runtime_invoke(init_logging_method, nullptr, init_logging_params, exc_obj);
    
    if (exc != nullptr)
    {
        log_error("mono_runtime_invoke {} failed with exception", k_entry_point_init_logging_method);
        MonoObject* exc0 = nullptr;
        MonoError error0;
        auto exc_mono_str = mono_object_try_to_string(reinterpret_cast<MonoObject*>(exc), &exc0, &error0);
        auto exc_msg = mono_string_chars_internal(exc_mono_str);
        log_error(L"{}", exc_msg);
        return false;
    }

    exc = nullptr;

    void* preprocess_params[3] = {
        *get_bundles_ptr(),
        get_glib_new0_ptr(),
        nullptr
    };

    mono_runtime_invoke(preprocess_method, nullptr, preprocess_params, exc_obj);

    if (exc != nullptr)
    {
        log_error("mono_runtime_invoke {} failed with exception", k_entry_point_preprocess_method);
        MonoObject* exc0 = nullptr;
        MonoError error0;
        auto exc_mono_str = mono_object_try_to_string(reinterpret_cast<MonoObject*>(exc), &exc0, &error0);
        auto exc_msg = mono_string_chars_internal(exc_mono_str);
        log_error(L"{}", exc_msg);
        return false;
    }
    
    exc = nullptr;

    mono_runtime_invoke(init_method, nullptr, nullptr, exc_obj);

    if (exc != nullptr)
    {
        log_error("mono_runtime_invoke {} failed with exception", k_entry_point_init_method);
        MonoObject* exc0 = nullptr;
        MonoError error0;
        auto exc_mono_str = mono_object_try_to_string(reinterpret_cast<MonoObject*>(exc), &exc0, &error0);
        auto exc_msg = mono_string_chars_internal(exc_mono_str);
        log_error(L"{}", exc_msg);
        return false;
    }

    g_already_init_managed = true;

    log_debug("CSharpLoader init success.");
    return true;
}


static bool bootstrap_late_init()
{
    auto image = mono_assembly_get_image(g_assembly);

    if (!image)
    {
        log_error("mono_assembly_get_image failed");
        return false;
    }

    auto late_init_method_desc = mono_method_desc_new(k_entry_point_late_init_method, true);
    defer([&] { mono_method_desc_free(late_init_method_desc); });

    if (!late_init_method_desc)
    {
        log_error("Invalid method descriptor: mono_method_desc_new failed for {}", k_entry_point_late_init_method);
        return false;
    }

    auto late_init_method = mono_method_desc_search_in_image(late_init_method_desc, image);

    if (!late_init_method)
    {
        log_error("Did not find the method `{}` mono_method_desc_search_in_image failed", k_entry_point_late_init_method);
        return false;
    }

    MonoException* exc = nullptr;
    MonoObject** exc_obj = reinterpret_cast<MonoObject**>(&exc);
    
    exc = nullptr;

    mono_runtime_invoke(late_init_method, nullptr, nullptr, exc_obj);

    if (exc != nullptr)
    {
        log_error("mono_runtime_invoke {} failed with exception", k_entry_point_late_init_method);
        MonoObject* exc0 = nullptr;
        MonoError error0;
        auto exc_mono_str = mono_object_try_to_string(reinterpret_cast<MonoObject*>(exc), &exc0, &error0);
        auto exc_msg = mono_string_chars_internal(exc_mono_str);
        log_error(L"{}", exc_msg);
        return false;
    }

    g_already_init_managed = true;

    log_debug("CSharpLoader late init success.");
    return true;
}


static void post_csharp_loader_x_load_runtime_x_callback()
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

    if (!bootstrap_init())
    {
        log_error("init_managed_mod_loader failed.");
        return;
    }

    log_debug("post_csharp_loader_x_load_runtime_x_callback completed successfully.");
}


static void post_csharp_loader_x_load_x_callback()
{
#ifdef LOAD_THREADED
    g_main_background_thread = CreateThread(nullptr, 0, mod_background_thread, g_hModule, 0, nullptr);
#else
    if (!bootstrap_late_init())
    {
        log_error("late_init_managed_mod_loader failed.");
        return;
    }
#endif

    log_debug("post_csharp_loader_x_load_x_callback completed successfully.");
}


static void post_load_assembly_bundles()
{
    log_debug("Assembly bundles callback triggered.");

    auto loader_dir = get_loader_dir();
    auto mod_dir = get_mod_dir();
    auto dirs = std::vector
    {
        loader_dir / L"Overrides",
        mod_dir / L"Overrides",
        loader_dir / "ReadyM.Loader.Wukong.Bootstrap.dll"
    };

    if (load_and_replace_assembly_bundles(dirs))
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

std::wstring utf8_to_wide(const std::string& str)
{
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(size_needed - 1, 0); // -1 to exclude null terminator
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
    return wstr;
}

std::wstring get_handshake_file_path()
{
    wchar_t* localAppData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &localAppData)))
    {
        std::wstring path = localAppData;
        CoTaskMemFree(localAppData);
        return path + L"\\ReadyM.Launcher\\wukong_handshake.env";
    }
    return L"";
}

static bool is_launcher_process_still_running(DWORD pid, const std::wstring& expectedImageName)
{
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProc)
        return false;

    wchar_t exePath[MAX_PATH];
    DWORD len = GetModuleFileNameEx(hProc, NULL, exePath, MAX_PATH);
    CloseHandle(hProc);

    if (len == 0)
        return false;

    // You can do stricter matching here if needed
    std::wstring actual = exePath;
    size_t pos = actual.find_last_of(L"\\/");
    std::wstring filename = (pos != std::wstring::npos) ? actual.substr(pos + 1) : actual;

    return _wcsicmp(filename.c_str(), expectedImageName.c_str()) == 0;
}

static std::unordered_map<std::wstring, std::wstring> parse_env_file(const std::wstring& path)
{
    std::unordered_map<std::wstring, std::wstring> map;

    std::ifstream file(path); // open as narrow stream (UTF-8)
    if (!file.is_open()) return map;

    std::string line;
    while (std::getline(file, line))
    {
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string keyStr = line.substr(0, pos);
        std::string valueStr = line.substr(pos + 1);

        std::wstring key = utf8_to_wide(keyStr);
        std::wstring value = utf8_to_wide(valueStr);

        map[key] = value;
    }

    return map;
}

static std::optional<std::wstring> try_get_mod_folder_override()
{
    auto path = get_handshake_file_path();
    auto env = parse_env_file(path);

    // print the env map for debugging
    log_debug(L"Parsed environment variables from {}:", path);
    for (const auto& [key, value] : env)
    {
        if (key == L"JWT_TOKEN")
            continue; // skip logging the JWT for security reasons
    
        log_debug(L"{}: {}", key, value);
    }

    if (env.contains(L"LAUNCHER_PID"))
    {
        DWORD pid = std::stoul(env[L"LAUNCHER_PID"]);
        if (!is_launcher_process_still_running(pid, L"ReadyM.Launcher.exe"))
        {
            log_error(L"Launcher process with PID {} is not running or does not match expected image name.", pid);
            return std::nullopt;
        }
    }

    log_debug(L"Launcher process is running as expected.");

    if (env.contains(L"MOD_FOLDER"))
    {
        std::wstring wmodfolder(env[L"MOD_FOLDER"].begin(), env[L"MOD_FOLDER"].end());
        if (std::filesystem::exists(wmodfolder))
            return wmodfolder;
    }

    return std::nullopt;
}

static bool init_pak_loading()
{
    log_info("Patching Unreal Engine .pak signature checks");
    if (!patch_pak_signature_check())
    {
        log_error("Failed to patch Unreal Engine .pak signature checks");
        return false;
    }

    log_info("Successfully patched Unreal Engine .pak signature checks to disable them.");
    return true;
}

static bool init_embed_runtime()
{
    if (!init_console_logging())
    {
        log_error("Failed to initialize logging.");
    }
    
    auto enable_console_flag = load_enable_console();

    if (enable_console_flag == 1)
    {
        create_console();
        log_info(L"ReadyM WukongMp C# Loader ver. {} {}", get_version_string(), get_title_string());
    }

    auto mod_dir_override = try_get_mod_folder_override();

    if (mod_dir_override.has_value())
    {
        set_mod_dir_override(mod_dir_override.value());
        log_debug(L"Mod folder override: {}", get_mod_dir().c_str());
    }
    else
    {
        log_debug(L"Mod folder: {}", get_mod_dir().c_str());
    }

    auto enable_jit_flag = load_enable_jit();
    auto enable_develop_flag = load_enable_develop();

    log_info("Intercepting USharp init.");
    if (!intercept_csharp_loader_x_load_runtimes(&post_csharp_loader_x_load_runtime_x_callback))
    {
        log_error("Failed to intercept USharp init.");
        return false;
    }

    if (!intercept_csharp_loader_x_load(&post_csharp_loader_x_load_x_callback))
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
    auto loader_dir = get_loader_dir();
    
    log_info("CSharpLoader EnableDevelop flag: {}", enable_develop_flag);
    if (enable_develop_flag)
    {
        auto dirs = std::vector
        {
            mod_dir / L"ReflectionOnly",
            loader_dir
        };
        
        if (!load_debugger_symbols(dirs))
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
    init_pak_loading();
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
