#include <cstdio>
#include <cstring>
#include <string>
#include <windows.h>
#include <tlhelp32.h>

#ifndef DEFAULT_DLL_NAME
#define DEFAULT_DLL_NAME "waw_modern.dll"
#endif

namespace {

DWORD FindProcessId(const char* exeName) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return 0;

  PROCESSENTRY32 pe{};
  pe.dwSize = sizeof(pe);
  DWORD pid = 0;

  if (Process32First(snap, &pe)) {
    do {
      if (_stricmp(pe.szExeFile, exeName) == 0) {
        pid = pe.th32ProcessID;
        break;
      }
    } while (Process32Next(snap, &pe));
  }

  CloseHandle(snap);
  return pid;
}

bool HasModuleLoaded(DWORD pid, const char* modName) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
  if (snap == INVALID_HANDLE_VALUE) return false;

  MODULEENTRY32 me{};
  me.dwSize = sizeof(me);
  bool found = false;

  if (Module32First(snap, &me)) {
    do {
      if (_stricmp(me.szModule, modName) == 0) {
        found = true;
        break;
      }
    } while (Module32Next(snap, &me));
  }

  CloseHandle(snap);
  return found;
}

std::string GetExeDirectory() {
  char buf[MAX_PATH]{};
  GetModuleFileNameA(nullptr, buf, MAX_PATH);
  std::string path(buf);
  const size_t pos = path.find_last_of("\\/");
  return (pos != std::string::npos) ? path.substr(0, pos) : ".";
}

} // namespace

int main(int argc, char** argv) {
  const char* targetExe = "CoDWaW.exe";
  std::string dllPath = (argc >= 2) ? argv[1] : (GetExeDirectory() + "\\" DEFAULT_DLL_NAME);

  char fullDllPath[MAX_PATH]{};
  if (!GetFullPathNameA(dllPath.c_str(), MAX_PATH, fullDllPath, nullptr)) {
    std::printf("[!] Invalid DLL path: %s\n", dllPath.c_str());
    return 1;
  }

  if (GetFileAttributesA(fullDllPath) == INVALID_FILE_ATTRIBUTES) {
    std::printf("[!] DLL not found: %s\n", fullDllPath);
    return 1;
  }

  const char* dllFileName = std::strrchr(fullDllPath, '\\');
  dllFileName = dllFileName ? (dllFileName + 1) : fullDllPath;

  std::printf("[*] Call of Duty: World at War SP Injector (CoDWaW.exe ONLY)\n");
  std::printf("[*] DLL: %s\n", fullDllPath);
  std::printf("[*] Waiting for CoDWaW.exe ...\n");

  DWORD pid = 0;
  while ((pid = FindProcessId(targetExe)) == 0) {
    Sleep(300);
  }
  std::printf("[+] Found CoDWaW.exe (PID: %lu)\n", static_cast<unsigned long>(pid));

  std::printf("[*] Waiting for d3d9.dll in target process...\n");
  for (int attempt = 0; attempt < 120; ++attempt) {
    if (HasModuleLoaded(pid, "d3d9.dll")) break;
    if (FindProcessId(targetExe) == 0) {
      std::printf("[!] Target process exited.\n");
      return 1;
    }
    Sleep(150);
  }

  if (HasModuleLoaded(pid, dllFileName)) {
    std::printf("[!] %s is already injected into CoDWaW.exe.\n", dllFileName);
    return 0;
  }

  HANDLE hProc = OpenProcess(
      PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
      FALSE, pid);
  if (!hProc) {
    std::printf("[!] OpenProcess failed (error %lu). Try running as Administrator if game is elevated.\n",
                GetLastError());
    return 1;
  }

  const size_t pathLen = std::strlen(fullDllPath) + 1;
  LPVOID remoteMem = VirtualAllocEx(hProc, nullptr, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!remoteMem) {
    std::printf("[!] VirtualAllocEx failed (error %lu)\n", GetLastError());
    CloseHandle(hProc);
    return 1;
  }

  if (!WriteProcessMemory(hProc, remoteMem, fullDllPath, pathLen, nullptr)) {
    std::printf("[!] WriteProcessMemory failed (error %lu)\n", GetLastError());
    VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return 1;
  }

  HMODULE k32 = GetModuleHandleA("kernel32.dll");
  auto loadLib = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(k32, "LoadLibraryA"));
  HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, loadLib, remoteMem, 0, nullptr);
  if (!hThread) {
    std::printf("[!] CreateRemoteThread failed (error %lu)\n", GetLastError());
    VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return 1;
  }

  WaitForSingleObject(hThread, 5000);
  DWORD exitCode = 0;
  GetExitCodeThread(hThread, &exitCode);

  CloseHandle(hThread);
  VirtualFreeEx(hProc, remoteMem, 0, MEM_RELEASE);
  CloseHandle(hProc);

  if (exitCode == 0) {
    std::printf("[!] LoadLibraryA returned NULL in CoDWaW.exe.\n");
    return 1;
  }

  std::printf("[+] Injected %s successfully! (Module base: 0x%08lX)\n",
              dllFileName, static_cast<unsigned long>(exitCode));
  std::printf("[*] Hotkeys: INSERT or F1 = Menu | F7 = Keybinds HUD | V = Noclip | END = Unload\n");
  Sleep(1500);
  return 0;
}
