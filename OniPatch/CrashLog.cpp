#include "pch.h"
#include "CrashLog.h"
#include "Cheats.h"

namespace
{
    // The game may raise and handle some exceptions itself; keep the log short.
    const LONG MAX_ENTRIES = 20;
    volatile LONG entries = 0;

    bool IsFatalCode(DWORD code)
    {
        switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
        case EXCEPTION_ILLEGAL_INSTRUCTION:
        case EXCEPTION_PRIV_INSTRUCTION:
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
        case EXCEPTION_STACK_OVERFLOW:
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            return true;
        }
        return false;
    }

    void ModuleOf(DWORD address, char *name, size_t size)
    {
        HMODULE module = NULL;
        strcpy_s(name, size, "?");
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(address), &module)) {
            char path[MAX_PATH];
            if (GetModuleFileNameA(module, path, MAX_PATH)) {
                const char *file = strrchr(path, '\\');
                sprintf_s(name, size, "%s+0x%lX", file ? file + 1 : path, address - reinterpret_cast<DWORD>(module));
            }
        }
    }

    LONG CALLBACK OnException(PEXCEPTION_POINTERS info)
    {
        DWORD code = info->ExceptionRecord->ExceptionCode;
        if (!IsFatalCode(code) || InterlockedIncrement(&entries) > MAX_ENTRIES)
            return EXCEPTION_CONTINUE_SEARCH;

        char dir[MAX_PATH];
        char path[MAX_PATH];
        if (GetEnvironmentVariableA("LOCALAPPDATA", dir, MAX_PATH) == 0)
            return EXCEPTION_CONTINUE_SEARCH;
        sprintf_s(path, "%s\\OniLauncher", dir);
        CreateDirectoryA(path, NULL);
        strcat_s(path, "\\crash.log");

        FILE *file = nullptr;
        if (fopen_s(&file, path, "a") != 0 || file == nullptr)
            return EXCEPTION_CONTINUE_SEARCH;

        const CONTEXT *c = info->ContextRecord;
        DWORD address = reinterpret_cast<DWORD>(info->ExceptionRecord->ExceptionAddress);
        char module[MAX_PATH];
        ModuleOf(address, module, sizeof(module));

        SYSTEMTIME now;
        GetLocalTime(&now);
        fprintf(file, "%04d-%02d-%02d %02d:%02d:%02d exception 0x%08lX at 0x%08lX (%s)\n",
            now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, code, address, module);
        if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2)
            fprintf(file, "  %s 0x%08lX\n", info->ExceptionRecord->ExceptionInformation[0] ? "write to" : "read from",
                static_cast<DWORD>(info->ExceptionRecord->ExceptionInformation[1]));
#ifdef _M_IX86
        fprintf(file, "  eax=%08lX ebx=%08lX ecx=%08lX edx=%08lX esi=%08lX edi=%08lX ebp=%08lX esp=%08lX\n",
            c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
        fprintf(file, "  stack:");
        const DWORD *stack = reinterpret_cast<const DWORD *>(c->Esp);
        for (int i = 0; i < 16; i++) {
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(stack + i, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT)
                break;
            fprintf(file, " %08lX", stack[i]);
        }
        fprintf(file, "\n");
#endif
        fprintf(file, "  cheats:");
        for (int i = 0; i < CHEAT_COUNT; i++)
            if (Cheats::Get().IsEnabled(static_cast<Cheat>(i)))
                fprintf(file, " F%d", i + 1);
        fprintf(file, "\n");
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(reinterpret_cast<LPCVOID>(0x033F9B24), &mbi, sizeof(mbi)) != 0 && mbi.State == MEM_COMMIT)
            fprintf(file, "  player state +4=%08lX action +2238=%02X\n",
                *reinterpret_cast<volatile DWORD *>(0x033F9B24), *reinterpret_cast<volatile BYTE *>(0x033FBD58));
        fclose(file);
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

void CrashLog::Install()
{
    AddVectoredExceptionHandler(1, OnException);
}
