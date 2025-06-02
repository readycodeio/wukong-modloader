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
#include "USharp/usharp.h"
#include "Windows/console.h"


static bool init_managed_mod_loader()
{
    void* domain = mono_get_root_domain();
    if (!domain)
    {
        log_error("mono_get_root_domain_ptr returned null.");
        return false;;
    }

    log_info("Domain initialized");

    void* assembly = mono_assembly_request_open(L"CSharpLoader\\CSharpManager.bin");
    if (!assembly)
    {
        log_error("mono_assembly_request_open returned null.");
        return false;;
    }

    auto res = ves_icall_System_AppDomain_ExecuteAssembly(domain, assembly);
    if (res != 0)
    {
        log_error("Assembly execution failed.");
        return false;
    }

    log_debug("CSharpLoader init success.");
    return true;
}


static void post_runtime_load_callback()
{
    if (!init_managed_mod_loader())
    {
        TerminateProcess(GetCurrentProcess(), 137);
    }
}


static bool init_embed_runtime()
{
    auto enable_console_flag = load_enable_console();
    auto enable_jit_flag = load_enable_jit();
    auto enable_develop_flag = load_enable_develop();

    if (enable_console_flag == 1)
    {
        create_console();
    }

    log_info("Intercepting USharp init.");
    if (!intercept_usharp_init(&post_runtime_load_callback))
    {
        log_error("Failed to intercept USharp init.");
        return false;
    }
    
    log_info("CSharpLoader EnableDevelop flag: {}", enable_develop_flag);
    if (enable_develop_flag)
    {
        if (!load_debugger_symbols(L"./CSharpLoader/Mods/ReflectionOnly"))
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


static DWORD wait_and_exit(DWORD code)
{
    Sleep(10000);
    return code;
}


DWORD WINAPI main_background_thread(LPVOID dwModule)
{
    if (!init_embed_runtime())
    {
        return wait_and_exit(EXIT_FAILURE);
    }
    
    return EXIT_SUCCESS;
}


static HANDLE g_main_background_thread = nullptr;


static void init_dll(HMODULE hModule)
{
    DisableThreadLibraryCalls(hModule);
    
    init_version_dll();
    g_main_background_thread = CreateThread(nullptr, 0, main_background_thread, hModule, 0, nullptr);
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
