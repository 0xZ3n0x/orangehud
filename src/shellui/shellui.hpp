/*
 * ShellUI module surface: build switches, domain globals, the HUD frame and
 * layout, and the Mono helpers / hooks the HUD code uses.
 */
#pragma once

// Standard library
#include <cstdint>
#include <string>

// Project
#include "mono/mono.hpp"

/*
 * Guards ~50 verbose diagnostic blocks in the ShellUI hooks. It was hardcoded
 * to 1, so those blocks shipped in release payloads — ShellUI is an injected
 * system process, and the chattiest logs in the project ran on every boot.
 *
 * Follows the build type via NDEBUG; override with -DSHELL_DEBUG=1 to get the
 * verbose blocks in a release build when chasing a report.
 */
#ifndef SHELL_DEBUG
#ifdef NDEBUG
#define SHELL_DEBUG 0
#else
#define SHELL_DEBUG 1
#endif
#endif

#define libSceKernelHandle ORANGE_LIBKERNEL_HANDLE
#define KERNEL_DLSYM(handle, sym) (*(void**)& sym = (void*)kernel_dynlib_dlsym(-1, handle, #sym))

typedef void* ScePthread;

/** Overlay configuration + current texts, as last published by the daemon. */
struct OrangeHudFrame;
extern OrangeHudFrame g_hud;

/** The corner panel's box, computed by apply_overlay_layout(). */
namespace orange::overlay
{
struct Layout;
}

using OverlayLayout = orange::overlay::Layout;
extern OverlayLayout g_overlay_layout;

void notify(const char* text, ...);

extern void (*OnRender_orig)(MonoObject* instance);

/* Mono assembly / type names (set at init). */
extern std::string appsystem_dll;

/* =============================== mono utils
 * =============================================================================*/
void Patch_Main_thread_Check(MonoImage* image_core);
void CheckRunningOnMainThread();
uint64_t Get_Address_of_Method(MonoImage* Assembly_Image, const char* Name_Space, const char* Class_Name, const char* Method_Name, int Param_Count);
uint64_t Get_Address_of_Method(MonoImage* Assembly_Image, MonoClass* klass, const char* Method_Name, int Param_Count);
/** Recompute g_overlay_layout from overlay edge and align. */
void apply_overlay_layout();
/** Apply the overlay layout using the live ShellUI logical canvas dimensions. */
void apply_overlay_layout(float screen_w, float screen_h);

/*
 * Property access: the accessor method is looked up once per (class, property)
 * and its JIT thunk is cached, so the per-frame HUD code does not re-resolve
 * (mono_class_get_property_from_name + mono_compile_method) on every call.
 */
MonoMethod* Property_Method(MonoClass* Klass, const char* Property_Name, bool setter);
uint64_t Property_Thunk(MonoClass* Klass, const char* Property_Name, bool setter);

template <typename result> result Get_Property(MonoClass* Klass, MonoObject* Instance, const char* Property_Name)
{
    const uint64_t thunk = Property_Thunk(Klass, Property_Name, false);
    if (!thunk)
    {
        return (result)0;
    }

    if (Instance != nullptr)
    {
        using Fn = result (*)(MonoObject*);
        return reinterpret_cast<Fn>(thunk)(Instance);
    }
    using Fn = result (*)();
    return reinterpret_cast<Fn>(thunk)();
}

template <typename result> result Get_Property(MonoImage* Assembly_Image, const char* Namespace, const char* Class_Name, MonoObject* Instance, const char* Property_Name)
{
    return Get_Property<result>(mono_class_from_name(Assembly_Image, Namespace, Class_Name), Instance, Property_Name);
}

template <typename Param> void Set_Property(MonoClass* Klass, MonoObject* Instance, const char* Property_Name, Param Value)
{
    if (Instance == nullptr)
    {
        return;
    }

    const uint64_t thunk = Property_Thunk(Klass, Property_Name, true);
    if (!thunk)
    {
        return;
    }

    using Fn = void (*)(MonoObject*, Param);
    reinterpret_cast<Fn>(thunk)(Instance, Value);
}

/* Same as Set_Property but through mono_runtime_invoke (reference/struct args). */
template <typename Param> void Set_Property_Invoke(MonoClass* Klass, MonoObject* Instance, const char* Property_Name, Param Value)
{
    if (Instance == nullptr)
    {
        return;
    }

    MonoMethod* Set_Method = Property_Method(Klass, Property_Name, true);
    if (Set_Method == nullptr)
    {
        return;
    }

    mono_runtime_invoke(Set_Method, Instance, (void**)&Value, 0);
}

template <typename result, typename... Args> result Invoke(MonoImage* Assembly_Image, MonoClass* klass, MonoObject* Instance, const char* Method_Name, Args... args)
{
    uint64_t ThunkAddress = Get_Address_of_Method(Assembly_Image, klass, Method_Name, (int)(sizeof...(args)));

    if (!ThunkAddress)
    {
        return (result)0;
    }

    if (Instance)
    {
        result (*Method)(MonoObject* Instance, Args... args) = decltype(Method)(ThunkAddress);
        return Method(Instance, args...);
    }
    else
    {
        result (*Method)(Args... args) = decltype(Method)(ThunkAddress);
        return Method(args...);
    }
}

/* ================================= ORIG HOOKED MONO FUNCS ============================================= */

/* ================================= HOOKED MONO FUNCS ============================================= */

extern MonoImage* pui_img;
extern MonoObject* Game;

MonoObject* CreateUIColor(float r, float g, float b, float a);
MonoObject* CreateUIFont(int size, int style, int weight);
MonoObject* CreateLabel(const char* name, float x, float y, const char* text, MonoObject* font, int horzAlign, int vertAlign, float r, float g, float b, float a);
void Widget_Append_Child(MonoObject* widget, MonoObject* child);
MonoObject* New_Object(MonoClass* Klass);
MonoImage* getDLLimage(const char* dll_file);

/* ================================= HOOKED MONO FUNCS ============================================= */
