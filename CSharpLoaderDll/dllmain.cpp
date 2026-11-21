// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <mutex>
#include <string>
#include "log.hpp"
#include "memory.h"
#include "overlay.hpp"
#include "paths.hpp"
#include "plugin.hpp"

enum class DllType {
    Unknown,
    Version,
    Hid,
};

static DllType dllType = DllType::Unknown;

struct MonoAssemblyOpenRequest
{
    uint8_t raw[40];
};

typedef enum {
    MONO_IMAGE_OK,
    MONO_IMAGE_ERROR_ERRNO,
    MONO_IMAGE_MISSING_ASSEMBLYREF,
    MONO_IMAGE_IMAGE_INVALID
} MonoImageOpenStatus;

// MonoThread * mono_thread_internal_attach (MonoDomain *domain)
typedef void* (*mono_thread_internal_attach_t)(void* domain);
// MonoAssembly * mono_assembly_request_open (const char *filename, const MonoAssemblyOpenRequest *open_req, MonoImageOpenStatus *status)
typedef void* (*mono_assembly_request_open_t)(const char* filename, const MonoAssemblyOpenRequest* open_req, MonoImageOpenStatus* status);

/*
gint32 ves_icall_System_AppDomain_ExecuteAssembly (
    MonoAppDomainHandle ad, MonoReflectionAssemblyHandle refass, MonoArrayHandle args, MonoError *error)
struct _MonoAppDomain { // size: 32
    MonoMarshalByRefObject mbr; // 0
    MonoDomain *data; // 24
};
struct _MonoReflectionAssembly { // size: 32
    MonoObject object; // 0
    MonoAssembly *assembly; // 16
    MonoObject *evidence; // 24
};
*/
struct _MonoAppDomain { // size: 32
    uint8_t mbr[24]; // 0
    void* data; // 24
};
struct _MonoReflectionAssembly { // size: 32
    uint8_t object[16]; // 0
    void* assembly; // 16
    void* evidence; // 24
};
struct MonoError {
    uint8_t raw[104];
};
typedef int (*ves_icall_System_AppDomain_ExecuteAssembly_t)(_MonoAppDomain** ad, _MonoReflectionAssembly** refass, void** args, MonoError* error);

// shows that the loader could not start, the details are in the log and the in-game console
static DWORD start_failed() {
    const std::wstring text = std::wstring(L"C# Loader could not start\nThe game may have been updated  \u00B7  ")
        + overlay_key_name() + L" shows the details";
    overlay_show_banner(text.c_str(), BannerError);
    return EXIT_FAILURE;
}

DWORD WINAPI MainThread(LPVOID dwModule)
{
    if (dllType == DllType::Version)
    {
        Sleep(3000);
    }
    // a full path: without one, GetPrivateProfile* looks in the Windows folder
    const std::wstring configPath = loader_path(L"b1cs.ini");
    const wchar_t* configFile = configPath.c_str();
    UINT enableConsole = GetPrivateProfileIntW(L"Settings", L"Console", 0, configFile);
    BOOL enableJit = GetPrivateProfileIntW(L"Settings", L"EnableJit", 1, configFile);
    UINT enableOverlay = GetPrivateProfileIntW(L"Settings", L"Overlay", 1, configFile);
    if (enableConsole == 1) {
        AllocConsole();
        FILE* fDummy;
        freopen_s(&fDummy, "CONIN$", "r", stdin);
        freopen_s(&fDummy, "CONOUT$", "w", stdout);
        freopen_s(&fDummy, "CONOUT$", "w", stderr);
    }
    if (enableOverlay == 1) {
        wchar_t overlayKey[32];
        GetPrivateProfileStringW(L"Settings", L"OverlayKey", L"F10", overlayKey, static_cast<DWORD>(std::size(overlayKey)), configFile);
        overlay_start(static_cast<HMODULE>(dwModule), overlayKey);
    }
    loadPluginDlls();
    log_info("CSharpLoader enableJit: %d", enableJit);
    log_info("CSharpLoader wait for init.");
    // enable jit
    if (enableJit) {
        uint64_t memory_function_ptr = signature("83 3D ? ? ? ? 00 0F 84 ? ? ? ? C7 84 24 ? ? 00 00 01 00 00 00").GetPointer();
        // "75 ? B9" at the end: without it, the pattern also matches other code in game build 23831
        uint64_t mono_mode_ptr = signature("48 8D 0D ? ? ? ? E8 ? ? ? ? 89 44 24 ? 83 7C 24 ? 00 75 ? B9").GetPointer();
        if (memory_function_ptr == 0) {
            log_error("memory function signature not found.");
        } else if (mono_mode_ptr == 0) {
            // patch both or none, half of the patch would leave the runtime in a mode nobody tested
            log_error("mono_mode signature not found.");
        } else {
            DWORD old_protect;
            if (VirtualProtect((void*)(memory_function_ptr + 7), 2, PAGE_EXECUTE_READWRITE, &old_protect)) {
                *(uint16_t*)(memory_function_ptr + 7) = 0xE990;  // nop; jmp
                VirtualProtect((void*)(memory_function_ptr + 7), 2, old_protect, &old_protect);
                if (VirtualProtect((void*)(mono_mode_ptr + 7), 5, PAGE_EXECUTE_READWRITE, &old_protect)) {
                    *(uint8_t*)(mono_mode_ptr + 7) = 0xB8;
                    *(uint32_t*)(mono_mode_ptr + 8) = 1;
                    VirtualProtect((void*)(mono_mode_ptr + 7), 5, old_protect, &old_protect);
                }
            }
        }
    }

    signature domain_s("F0 FF 88 B0 00 00 00 48 8B 05 ? ? ? ? 48 3B D8 49 0F 44 C4");
    if (domain_s.GetPointer() == 0) {
        log_error("domainPtr pattern not found.");
        return start_failed();
    }
    void **domainPtr = (void**)domain_s.instruction(10).add(14).GetPointer();
    if (domainPtr == nullptr) {
        log_error("domainPtr not found.");
        return start_failed();
    }
    auto mono_thread_internal_attach = (mono_thread_internal_attach_t)signature(
        "40 57 48 83 EC 30 8B 15 ? ? ? ? 48 8B F9 65 48 8B 04 25 58 00 00 00 B9 A8 02 00 00 48 8B 04 D0 48 83 3C 01 00").GetPointer();
    if (mono_thread_internal_attach == nullptr) {
        log_error("mono_thread_internal_attach not found.");
        return start_failed();
    }
    auto mono_assembly_request_open = (mono_assembly_request_open_t)signature(
        "40 55 41 56 41 57 48 8D AC 24 40 FF FF FF 48 81 EC C0 01 00 00 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 90 00 00 00 48 89 4D 90").GetPointer();
    if (mono_assembly_request_open == nullptr) {
        log_error("mono_assembly_request_open not found.");
        return start_failed();
    }
    auto ves_icall_System_AppDomain_ExecuteAssembly = (ves_icall_System_AppDomain_ExecuteAssembly_t)signature(
        "48 89 5C 24 08 55 56 57 41 56 41 57 48 83 EC 50 48 89 94 24 88 00 00 00").GetPointer();
    if (ves_icall_System_AppDomain_ExecuteAssembly == nullptr) {
        log_error("ves_icall_System_AppDomain_ExecuteAssembly not found.");
        return start_failed();
    }
    void* domain = nullptr;
    for (size_t i = 0; i < 180; i++) {
        domain = *domainPtr;
        if (domain != nullptr) {
            break;
        }
        Sleep(1000); // 1s
    }
    if (domain == nullptr) {
        log_error("domain is null.");
        return start_failed();
    }
    Sleep(2000); // 2s

    void* mono_thread = mono_thread_internal_attach(domain);
    if (mono_thread == nullptr) {
        log_error("mono_thread_internal_attach failed.");
        return start_failed();
    }

    MonoAssemblyOpenRequest open_request{};
    MonoImageOpenStatus status = MonoImageOpenStatus::MONO_IMAGE_OK;

    const std::wstring fullFilename = loader_path(L"CSharpManager.bin");
    // convert to utf-8 to support Chinese path
    const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, fullFilename.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string fullFilenameA(utf8Size > 0 ? static_cast<size_t>(utf8Size) : 0, '\0');
    if (utf8Size <= 0 || WideCharToMultiByte(CP_UTF8, 0, fullFilename.c_str(), -1, &fullFilenameA[0], utf8Size, nullptr, nullptr) == 0) {
        log_error("convert path to utf-8 failed.");
        return start_failed();
    }
    void* assembly = mono_assembly_request_open(fullFilenameA.c_str(), &open_request, &status);
    if (assembly == nullptr) {
        log_error("mono_assembly_request_open failed.");
        return start_failed();
    }

    _MonoAppDomain ad{};
    ad.data = domain;
    _MonoReflectionAssembly refass{};
    refass.assembly = assembly;
    MonoError mono_error{};
    _MonoAppDomain* p_ad = &ad;
    _MonoReflectionAssembly* p_refass = &refass;
    void* args = nullptr;
    int res = ves_icall_System_AppDomain_ExecuteAssembly(&p_ad, &p_refass, &args, &mono_error);
    if (res != 0) {
        log_error("ves_icall_System_AppDomain_ExecuteAssembly failed: %d", res);
        return start_failed();
    }
    log_info("CSharpLoader init success.");
    return EXIT_SUCCESS;
}

void init_dll(HMODULE hModule) {
    DisableThreadLibraryCalls(hModule);
    wchar_t moduleFullpathFilename[MAX_PATH + 1];
    GetModuleFileNameW(hModule, moduleFullpathFilename, static_cast<UINT>(std::size(moduleFullpathFilename)));
    wchar_t fname[_MAX_FNAME+1];
    {
        wchar_t drive[_MAX_DRIVE+1];
        wchar_t dir[_MAX_DIR+1];
        wchar_t ext[_MAX_EXT+1];
        _wsplitpath_s(moduleFullpathFilename, drive, dir, fname, ext);
    }
    // the real version.dll or hid.dll is loaded on the first call of one of its functions, not here
    if (_wcsicmp(fname, L"version") == 0) {
        dllType = DllType::Version;
    } else if (_wcsicmp(fname, L"hid") == 0) {
        dllType = DllType::Hid;
    }
    HANDLE thread = CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
    if (thread != nullptr) {
        CloseHandle(thread);
    }
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    static std::once_flag initFlag;
    // nothing to do at DLL_PROCESS_DETACH: FreeLibrary there is not safe, and the process ends anyway
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        std::call_once(initFlag, [&]() { init_dll(hModule); });
    }
    return TRUE;
}

