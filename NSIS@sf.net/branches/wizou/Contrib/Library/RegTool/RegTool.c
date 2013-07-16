#include <windows.h>
#include "../../ExDll/nsis_tchar.h"

#define STR_SIZE 1024

void RegFile(TCHAR cmd, TCHAR *file, int x64);
void RegDll(TCHAR *file);
void RegTypeLib(TCHAR *file);
void DeleteFileOnReboot(TCHAR *pszFile);

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
  TCHAR *cmdline;
  TCHAR seekchar = _T(' ');

  cmdline = GetCommandLine();
  if (*cmdline == _T('\"'))
    seekchar = *cmdline++;

  while (*cmdline && *cmdline != seekchar)
    cmdline = CharNext(cmdline);
  cmdline = CharNext(cmdline);
  while (*cmdline == _T(' '))
    cmdline++;

  if (*cmdline++ != _T('/'))
  {
    ExitProcess(1);
    return 0;
  }

  if (*cmdline == _T('S'))
  {
    HKEY rootkey;

    if (SUCCEEDED(RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\NSIS.Library.RegTool.v3"), 0, KEY_READ, &rootkey)))
    {
      TCHAR keyname[STR_SIZE];

      while (RegEnumKey(rootkey, 0, keyname, STR_SIZE) == ERROR_SUCCESS)
      {
        HKEY key;

        if (SUCCEEDED(RegOpenKeyEx(rootkey, keyname, 0, KEY_READ, &key)))
        {
          DWORD t, count, l = sizeof(DWORD);

          if (SUCCEEDED(RegQueryValueEx(key, _T("count"), NULL, &t, (LPBYTE) &count, &l)) && t == REG_DWORD)
          {
            DWORD j;
            TCHAR valname[128], mode[3], file[STR_SIZE];

            for (j = 1; j <= count; j++)
            {
              wsprintf(valname, _T("%u.mode"), j);
              l = sizeof(mode);
              if (FAILED(RegQueryValueEx(key, valname, NULL, &t, (LPBYTE) mode, &l)) || t != REG_SZ)
                continue;

              wsprintf(valname, _T("%u.file"), j);
              l = STR_SIZE;
              if (FAILED(RegQueryValueEx(key, valname, NULL, &t, (LPBYTE) file, &l)) || t != REG_SZ)
                continue;

              RegFile(mode[0], file, mode[1] == _T('X'));
            }
          }

          RegCloseKey(key);
          RegDeleteKey(rootkey, keyname);
        }
      }

      RegCloseKey(rootkey);
      RegDeleteKey(HKEY_LOCAL_MACHINE, _T("Software\\NSIS.Library.RegTool.v3"));
    }

    {
      TCHAR file[STR_SIZE];
      if (GetModuleFileName(GetModuleHandle(NULL), file, STR_SIZE))
      {
        DeleteFileOnReboot(file);
      }
    }
  }
  else
  {
    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
    OleInitialize(NULL);

    if (*cmdline == _T('D'))
    {
      RegDll(cmdline + 1);
    }
    else if (*cmdline == _T('T'))
    {
      RegTypeLib(cmdline + 1);
    }

    OleUninitialize();
    SetErrorMode(0);
  }

  ExitProcess(0);
  return 0;
}

void SafeWow64EnableWow64FsRedirection(BOOL Wow64FsEnableRedirection)
{
  HMODULE kernel = GetModuleHandle(_T("kernel32"));
  if (kernel)
  {
    FARPROC proc = GetProcAddress(kernel, "Wow64EnableWow64FsRedirection");
    if (proc)
    {
      typedef BOOL (WINAPI *Wow64EnableWow64FsRedirectionPtr)(BOOL);
      Wow64EnableWow64FsRedirectionPtr Wow64EnableWow64FsRedirectionFunc =
        (Wow64EnableWow64FsRedirectionPtr) proc;

      Wow64EnableWow64FsRedirectionFunc(Wow64FsEnableRedirection);
    }
  }
}

void RegFile(TCHAR cmd, TCHAR *file, int x64)
{
  TCHAR self[STR_SIZE];
  TCHAR cmdline[STR_SIZE];

  int ready = 0;

  if (!*file || (cmd != _T('D') && cmd != _T('T') && cmd != _T('E')))
    return;

  if (cmd == _T('E'))
  {
    wsprintf(cmdline, _T("\"%s\" /regserver"), file);
    ready++;
  }
  else if (!x64)
  {
    if (GetModuleFileName(GetModuleHandle(NULL), self, STR_SIZE))
    {
      wsprintf(cmdline, _T("\"%s\" /%c%s"), self, cmd, file);
      ready++;
    }
  }
  else
  {
    if (GetSystemDirectory(self, STR_SIZE))
    {
      wsprintf(cmdline, _T("\"%s\\regsvr32.exe\" /s \"%s\""), self, file);
      ready++;

      SafeWow64EnableWow64FsRedirection(FALSE);
    }
  }

  if (ready)
  {
    PROCESS_INFORMATION pi;
    STARTUPINFO si = { sizeof(STARTUPINFO) };

    if (CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
      CloseHandle(pi.hThread);

      WaitForSingleObject(pi.hProcess, INFINITE);

      CloseHandle(pi.hProcess);
    }
    
    if (x64)
    {
      SafeWow64EnableWow64FsRedirection(TRUE);
    }
  }
}

void RegDll(TCHAR *file)
{
  HMODULE mod = LoadLibraryEx(file, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (mod)
  {
    FARPROC regfunc = GetProcAddress(mod, "DllRegisterServer");
    if (regfunc)
      regfunc();
    FreeLibrary(mod);
  }
}

void RegTypeLib(TCHAR *file)
{
  WCHAR wfile[STR_SIZE];

  if (MultiByteToWideChar(CP_ACP, 0, file, -1, wfile, STR_SIZE) != 0)
  {
    ITypeLib* tlib;
    if (SUCCEEDED(LoadTypeLib(wfile, &tlib))) {
      RegisterTypeLib(tlib, wfile, NULL);
      tlib->lpVtbl->Release(tlib);
    }
  }
}

TCHAR *mystrstri(TCHAR *a, TCHAR *b)
{
  int l = lstrlen(b);
  while (lstrlen(a) >= l)
  {
    TCHAR c = a[l];
    a[l] = 0;
    if (!lstrcmpi(a, b))
    {
      a[l] = c;
      return a;
    }
    a[l] = c;
    a = CharNext(a);
  }
  return NULL;
}

void mini_memcpy(void *out, const void *in, int len)
{
  TCHAR *c_out=(TCHAR*)out;
  TCHAR *c_in=(TCHAR *)in;
  while (len-- > 0)
  {
    *c_out++=*c_in++;
  }
}

void DeleteFileOnReboot(TCHAR *pszFile)
{
  BOOL fOk = 0;
  HMODULE hLib=GetModuleHandle(_T("KERNEL32.dll"));
  if (hLib)
  {
    typedef BOOL (WINAPI *mfea_t)(LPCTSTR lpExistingFileName,LPCTSTR lpNewFileName,DWORD dwFlags);
    mfea_t mfea;
    mfea=(mfea_t) GetProcAddress(hLib,_CRT_STRINGIZE(MoveFileEx));
    if (mfea)
    {
      fOk=mfea(pszFile, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    }
  }

  if (!fOk)
  {
    static TCHAR szRenameLine[1024];
    static TCHAR wininit[1024];
    int cchRenameLine;
    TCHAR *szRenameSec = _T("[Rename]\r\n");
    HANDLE hfile, hfilemap;
    DWORD dwFileSize, dwRenameLinePos;

    int spn;

    // wininit is used as a temporary here
    spn = GetShortPathName(pszFile,wininit,1024);
    if (!spn || spn > 1024)
      return;
    cchRenameLine = wsprintf(szRenameLine,_T("NUL=%s\r\n"),wininit);

    GetWindowsDirectory(wininit, 1024-16);
    lstrcat(wininit, _T("\\wininit.ini"));
    hfile = CreateFile(wininit,
        GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

    if (hfile != INVALID_HANDLE_VALUE)
    {
      dwFileSize = GetFileSize(hfile, NULL);
      hfilemap = CreateFileMapping(hfile, NULL, PAGE_READWRITE, 0, dwFileSize + cchRenameLine + 10, NULL);

      if (hfilemap != NULL)
      {
        LPTSTR pszWinInit = (LPTSTR) MapViewOfFile(hfilemap, FILE_MAP_WRITE, 0, 0, 0);

        if (pszWinInit != NULL)
        {
          LPTSTR pszRenameSecInFile = mystrstri(pszWinInit, szRenameSec);
          if (pszRenameSecInFile == NULL)
          {
            lstrcpy(pszWinInit+dwFileSize, szRenameSec);
            dwFileSize += 10;
            dwRenameLinePos = dwFileSize;
          }
          else
          {
            TCHAR *pszFirstRenameLine = pszRenameSecInFile+10;
            TCHAR *pszNextSec = mystrstri(pszFirstRenameLine,_T("\n["));
            if (pszNextSec)
            {
              TCHAR *p = ++pszNextSec;
              while (p < pszWinInit + dwFileSize) {
                p[cchRenameLine] = *p;
                p++;
              }

              dwRenameLinePos = pszNextSec - pszWinInit;
            }
            // rename section is last, stick item at end of file
            else dwRenameLinePos = dwFileSize;
          }

          mini_memcpy(&pszWinInit[dwRenameLinePos], szRenameLine, cchRenameLine);
          dwFileSize += cchRenameLine;

          UnmapViewOfFile(pszWinInit);
        }
        CloseHandle(hfilemap);
      }
      SetFilePointer(hfile, dwFileSize, NULL, FILE_BEGIN);
      SetEndOfFile(hfile);
      CloseHandle(hfile);
    }
  }
}
