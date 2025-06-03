#include <iterator>
#include <windows.h>

#include "Config/debugger.h"
#include "Config/flags.h"
#include "EntryDll/version_dll.h"
#include "Logger/logger.h"
#include "Mono/appdomain.h"
#include "Mono/assembly.h"
#include "Mono/jit.h"
#include "Mono/mono-debug.h"
#include "Mono/threads.h"
#include "USharp/usharp.h"
#include "Windows/console.h"


static void* g_domain;
static void* g_assembly;


static bool init_managed_mod_loader()
{
    auto mono_thread = mono_thread_internal_attach(g_domain);
    if (!mono_thread)
    {
        log_error("mono_thread_internal_attach failed.");
        return false;
    }

    auto res = ves_icall_System_AppDomain_ExecuteAssembly(g_domain, g_assembly);
    if (res != 0)
    {
        log_error("Assembly execution failed.");
        return false;
    }

    log_debug("CSharpLoader init success.");
    return true;
}


static HANDLE g_main_background_thread = nullptr;
static HMODULE g_hModule = nullptr;


static DWORD wait_and_exit(DWORD code)
{
    Sleep(10000);
    return code;
}


DWORD WINAPI mod_background_thread(LPVOID dwModule)
{
    if (!init_managed_mod_loader())
    {
        return wait_and_exit(EXIT_FAILURE);
    }
    
    return EXIT_SUCCESS;
}


static void post_runtime_load_callback()
{
    g_domain = mono_get_root_domain();
    if (!g_domain)
    {
        log_error("mono_get_root_domain returned null.");
        return;
    }

    log_info("Domain initialized");

    g_assembly = mono_assembly_request_open(std::filesystem::path(L"CSharpLoader") / L"EmbedCSharpLoader.Managed.bin");
    if (!g_assembly)
    {
        log_error("mono_assembly_request_open failed.");
        return;
    }

    load_assembly_bundles(std::filesystem::path(L"CSharpLoader") / L"Mods" / L"Overrides");

    g_main_background_thread = CreateThread(nullptr, 0, mod_background_thread, g_hModule, 0, nullptr);
}


static bool init_embed_runtime()
{
    auto enable_console_flag = load_enable_console();

    if (enable_console_flag == 1)
    {
        create_console();
        log_info(L"CSharp Embed Loader: v0.1");
    }
    
    auto enable_jit_flag = load_enable_jit();
    auto enable_develop_flag = load_enable_develop();

    log_info("Intercepting USharp init.");
    if (!intercept_usharp_init(&post_runtime_load_callback))
    {
        log_error("Failed to intercept USharp init.");
        return false;
    }
    
    log_info("CSharpLoader EnableDevelop flag: {}", enable_develop_flag);
    if (enable_develop_flag)
    {
        if (!load_debugger_symbols(std::filesystem::path(L"CSharpLoader") / L"Mods" / L"ReflectionOnly"))
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


void wait_for_cwd()
{
    wchar_t cwd[MAX_PATH];
    char cwdFull[MAX_PATH];

    do
    {
        Sleep(100);
        GetCurrentDirectory(MAX_PATH, cwd);
        WideCharToMultiByte(CP_UTF8, 0, cwd, MAX_PATH, cwdFull, MAX_PATH, NULL, NULL);
    }
    while (strstr(cwdFull, "Win64") == nullptr);
}


void print_cwd()
{
    wchar_t cwd[MAX_PATH];
    char cwdFull[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, cwd);
    WideCharToMultiByte(CP_UTF8, 0, cwd, MAX_PATH, cwdFull, MAX_PATH, NULL, NULL);
    std::cout << "Current working directory: " << cwdFull << std::endl;
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
    if (g_main_background_thread != nullptr)
        TerminateThread(g_main_background_thread, 0);
    
    deinit_version_dll();
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
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
