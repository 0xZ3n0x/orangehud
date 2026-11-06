// Standard library
#include <cstdint>
#include <string>

// SDK
#include <ps5/kernel.h>
#include <unistd.h>

// Project
#include "boot/hook_lifecycle.hpp"
#include "contract/hud_frame.h"
#include "detour/detour.h"
#include "platform/log.h"
#include "platform/notify.h"
#include "platform/ready.hpp"
#include "proc/authid_guard.hpp"
#include "proc/proc.h"
#include "proc/ucred.h"
#include "shellui.hpp"

// ---------------------------------------------------------------------------
// Globals (referenced by other shellui translation units)
// ---------------------------------------------------------------------------

std::string appsystem_dll;

MonoImage* pui_img = nullptr;
MonoObject* Game = nullptr;

bool hooked = false;

void (*OnRender_orig)(MonoObject* instance);

// Defined in hud/frame_hook.cpp
void OnRender_Hook(MonoObject* instance);

// ---------------------------------------------------------------------------
// Init constants
// ---------------------------------------------------------------------------

namespace
{

// Same magnitude as the previous sleep(0x100000) keep-alive.
constexpr unsigned kKeepAliveSleepSec = 0x100000;

// ---------------------------------------------------------------------------
// Mono images used during init
// ---------------------------------------------------------------------------

struct ShellImages
{
    MonoImage* app_system = nullptr;
    MonoImage* core = nullptr;
    MonoImage* pui = nullptr;
};

MonoImage* require_dll(const char* name)
{
    MonoImage* img = getDLLimage(name);
    if (!img)
    {
        LOG_ERROR("Failed to load Mono assembly: %s", name);
        notify("Failed to load assembly");
    }
    return img;
}

// ---------------------------------------------------------------------------
// Phase 1: native dynlib symbols
// ---------------------------------------------------------------------------

bool resolve_native_symbols(pid_t pid)
{
    int libkernelsys = get_module_handle(pid, "libkernel_sys.sprx");
    int (*sceKernelMprotect)(void* addr, size_t len, int prot) = nullptr;
    KERNEL_DLSYM(libkernelsys, sceKernelMprotect);
    detour_set_mprotect(sceKernelMprotect);
    KERNEL_DLSYM(libkernelsys, sceKernelSendNotificationRequest);

    /* platform must not CALL sceKernelSendNotificationRequest by name —
     * here that symbol is a dlsym'd *function pointer*. Register a trampoline. */
    notify_set_send(+[](int32_t device, void* req, size_t size, int32_t blocking) -> int32_t {
        if (!sceKernelSendNotificationRequest)
        {
            return -1;
        }
        return sceKernelSendNotificationRequest(device, static_cast<OrbisNotificationRequest*>(req), static_cast<int>(size), blocking);
    });

    return true;
}

// ---------------------------------------------------------------------------
// Phase 2: mono runtime symbols
// ---------------------------------------------------------------------------

bool resolve_mono_symbols(pid_t pid)
{
    int libmono = get_module_handle(pid, "libmonosgen-2.0.sprx");

    // Required: every symbol below is called by the HUD code.
    KERNEL_DLSYM(libmono, mono_get_root_domain);
    KERNEL_DLSYM(libmono, mono_domain_get);
    KERNEL_DLSYM(libmono, mono_thread_attach);
    KERNEL_DLSYM(libmono, mono_assembly_get_image);
    KERNEL_DLSYM(libmono, mono_domain_assembly_open);
    KERNEL_DLSYM(libmono, mono_class_from_name);
    KERNEL_DLSYM(libmono, mono_class_get_method_from_name);
    KERNEL_DLSYM(libmono, mono_class_get_property_from_name);
    KERNEL_DLSYM(libmono, mono_property_get_get_method);
    KERNEL_DLSYM(libmono, mono_property_get_set_method);
    KERNEL_DLSYM(libmono, mono_compile_method);
    KERNEL_DLSYM(libmono, mono_runtime_invoke);
    KERNEL_DLSYM(libmono, mono_object_new);
    KERNEL_DLSYM(libmono, mono_object_unbox);
    KERNEL_DLSYM(libmono, mono_runtime_object_init);
    KERNEL_DLSYM(libmono, mono_string_new);

    // Optional: kept resolved for future HUD widgets; a miss is not fatal.
    KERNEL_DLSYM(libmono, mono_object_to_string);
    KERNEL_DLSYM(libmono, mono_string_to_utf8);
    KERNEL_DLSYM(libmono, mono_free);
    KERNEL_DLSYM(libmono, mono_object_get_class);
    KERNEL_DLSYM(libmono, mono_gchandle_new);
    KERNEL_DLSYM(libmono, mono_class_vtable);
    KERNEL_DLSYM(libmono, mono_class_get_field_from_name);
    KERNEL_DLSYM(libmono, mono_field_static_set_value);

    const bool ok = mono_get_root_domain && mono_domain_get && mono_thread_attach && mono_assembly_get_image && mono_domain_assembly_open && mono_class_from_name && mono_class_get_method_from_name &&
                    mono_class_get_property_from_name && mono_property_get_get_method && mono_property_get_set_method && mono_compile_method && mono_runtime_invoke && mono_object_new &&
                    mono_object_unbox && mono_runtime_object_init && mono_string_new;

    if (!ok)
    {
        LOG_ERROR("Failed to resolve mono symbols");
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Phase 3: assembly / type name strings
// ---------------------------------------------------------------------------

void init_resource_names()
{
    appsystem_dll = "Sce.Vsh.ShellUI.AppSystem.dll";
}

// ---------------------------------------------------------------------------
// Phase 4: load Mono assemblies
// ---------------------------------------------------------------------------

bool load_shell_images(ShellImages& out)
{
    out.app_system = require_dll(appsystem_dll.c_str());
    out.core = require_dll("Sce.PlayStation.Core.dll");
    out.pui = require_dll("Sce.PlayStation.PUI.dll");

    // Publish images other modules need
    pui_img = out.pui;

    return out.app_system && out.core && out.pui;
}

// ---------------------------------------------------------------------------
// Phase 5: resolve Game container scene
// ---------------------------------------------------------------------------

bool resolve_game_container(MonoImage* app_system)
{
    MonoClass* layer_manager = mono_class_from_name(app_system, "Sce.Vsh.ShellUI.AppSystem", "LayerManager");
    if (!layer_manager)
    {
        notify("Failed to get LayerManager class");
        return false;
    }

    MonoMethod* find = mono_class_get_method_from_name(layer_manager, "FindContainerSceneByPath", 1);
    if (!find)
    {
        notify("Failed to get FindContainerSceneByPath method");
        return false;
    }

    MonoString* path_arg = mono_string_new(mono_domain_get(), "Game");
    void* args[1] = {path_arg};
    MonoObject* exception = nullptr;
    Game = mono_runtime_invoke(find, nullptr, args, &exception);

    if (exception)
    {
        notify("Exception occurred while calling FindContainerSceneByPath");
        return false;
    }
    if (!Game)
    {
        notify("Failed to get Game ContainerScene");
        return false;
    }

    LOG_DEBUG("Game ContainerScene: %p", static_cast<void*>(Game));
    return true;
}

// ---------------------------------------------------------------------------
// Phase 7: install hooks (table-driven mono methods + special cases)
// ---------------------------------------------------------------------------

/** Mono method hook descriptor; `orig` is address of the typed trampoline ptr. */
struct MonoHookSpec
{
    const char* name;
    MonoImage* image;
    const char* name_space;
    const char* klass;
    const char* method;
    int argc;
    void* hook;
    void** orig; // &SomeOrigFn
    bool required;
};

bool install_mono_hook(const MonoHookSpec& h)
{
    const uint64_t addr = Get_Address_of_Method(h.image, h.name_space, h.klass, h.method, h.argc);
    if (!addr)
    {
        LOG_ERROR("Hook target missing: %s", h.name);
        if (h.required)
        {
            notify("Failed to find hook target");
        }
        return !h.required;
    }
    LOG_DEBUG("Installing mono hook: %s target=%#02lx hook=%p", h.name, addr, h.hook);
    if (!InstallDetour(addr, h.hook, h.orig))
    {
        LOG_ERROR("Detour failed: %s", h.name);
        if (h.required)
        {
            notify("Failed to install hook");
        }
        return !h.required;
    }
    return true;
}

bool install_hooks(const ShellImages& img)
{
    Patch_Main_thread_Check(img.core);

    /*
     * The per-frame entry point is the only hook the HUD needs. It is armed last
     * so the UI thread cannot enter Orange code before the install transaction
     * has committed.
     */
    if (!install_mono_hook(
            {"PUI.Application.Update", img.pui, "Sce.PlayStation.PUI", "Application", "Update", 0, reinterpret_cast<void*>(&OnRender_Hook), reinterpret_cast<void**>(&OnRender_orig), true}))
    {
        return false;
    }
    return OnRender_orig != nullptr;
}

// ---------------------------------------------------------------------------
// Phase 6: keep-alive
// ---------------------------------------------------------------------------

void run_keep_alive()
{
    // Publish the initialized SceShellUI process instance.  The daemon keeps
    // this marker so a restart cannot inject a second copy into the same PID.
    orange::kHudReady.signal(getpid(), ORANGE_HUD_ABI);

    while (true)
    {
        LOG_DEBUG("sleeping ....");
        sleep(kKeepAliveSleepSec);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// main — orchestration only
// ---------------------------------------------------------------------------

int main(int argc, char const* argv[])
{
    (void)argc;
    (void)argv;

    if (hooked)
    {
        return 0;
    }

    const pid_t pid = getpid();
    orange::AuthIdGuard auth(pid, set_ucred_to_ptrace());

    if (!resolve_native_symbols(pid))
    {
        return -1;
    }

    LOG_DEBUG("Starting ShellUI Module ....");

    if (!resolve_mono_symbols(pid))
    {
        return -1;
    }

    init_resource_names();

    if (!mono_get_root_domain)
    {
        return -1;
    }

    Root_Domain = mono_get_root_domain();
    if (!Root_Domain)
    {
        LOG_ERROR("failed to get shellui root domain");
        return -1;
    }
    LOG_DEBUG("Shellui Root Domain: %p", static_cast<void*>(Root_Domain));
    mono_thread_attach(Root_Domain);

    ShellImages images;
    if (!load_shell_images(images))
    {
        return -1;
    }
    if (!resolve_game_container(images.app_system))
    {
        return -1;
    }

    LOG_DEBUG("Starting hooking...");
    shellui_hooks_begin_install();
    if (!install_hooks(images))
    {
        shellui_hooks_publish_failed();
        return -1;
    }

    LOG_DEBUG("Performing Magic ....");

    // Drop elevated auth before long-running work
    set_proc_authid(pid, auth.old_authid);
    auth.release();

    LOG_DEBUG("Performed Magic");

    shellui_hooks_publish_ready();
    hooked = true;
    run_keep_alive();
    return 0;
}
