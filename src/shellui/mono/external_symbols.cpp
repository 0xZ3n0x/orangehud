// Project
#include "mono.hpp"

/* ====================================== Global vars ======================================================*/
MonoDomain* Root_Domain = nullptr;

// Global function pointers

/* ====================================== Dynamic libkernel_sys Symbols ===================================*/
int (*sceKernelSendNotificationRequest)(int unk1, OrbisNotificationRequest* req, int size, int unk2) = nullptr;

/* ====================================== Dynamic Mono Symbols ===================================*/
MonoString* (*mono_object_to_string)(MonoObject* obj, MonoObject** exc) = nullptr;
uint32_t (*mono_gchandle_new)(MonoObject* obj, int pinned) = nullptr;
uint64_t (*mono_compile_method)(MonoMethod* method) = nullptr;
const char* (*mono_string_to_utf8)(MonoString* str) = nullptr;
void* (*mono_object_unbox)(MonoObject* obj) = nullptr;
void (*mono_free)(void* ptr) = nullptr;
MonoDomain* (*mono_get_root_domain)() = nullptr;
MonoClass* (*mono_class_from_name)(MonoImage* image, const char* name_space, const char* name) = nullptr;
MonoAssembly* (*mono_domain_assembly_open)(MonoDomain* domain, const char* name) = nullptr;
MonoImage* (*mono_assembly_get_image)(MonoAssembly* assembly) = nullptr;
MonoMethod* (*mono_property_get_get_method)(MonoProperty* prop) = nullptr;
MonoMethod* (*mono_property_get_set_method)(MonoProperty* prop) = nullptr;
MonoProperty* (*mono_class_get_property_from_name)(MonoClass* klass, const char* name) = nullptr;             //
MonoObject* (*mono_runtime_invoke)(MonoMethod* method, void* obj, void** params, MonoObject** exc) = nullptr; //
MonoString* (*mono_string_new)(MonoDomain* domain, const char* str) = nullptr;                                //
MonoThread* (*mono_thread_attach)(MonoDomain* domain) = nullptr;
MonoMethod* (*mono_class_get_method_from_name)(MonoClass* klass, const char* name, int param_count) = nullptr; //
void (*mono_runtime_object_init)(MonoObject* obj) = nullptr;
MonoClassField* (*mono_class_get_field_from_name)(MonoClass* klass, const char* name) = nullptr;
void (*mono_field_static_set_value)(MonoVTable* vt, MonoClassField* field, void* value) = nullptr;
MonoVTable* (*mono_class_vtable)(MonoDomain* domain, MonoClass* klass) = nullptr;
MonoDomain* (*mono_domain_get)() = nullptr;
MonoObject* (*mono_object_new)(MonoDomain* domain, MonoClass* klass) = nullptr;
MonoClass* (*mono_object_get_class)(MonoObject* obj) = nullptr;
