/*
  Copyright (c) 2002 Robert Rainwater
  Contributors: Justin Frankel, Fritz Elfert, Amir Szekely, Sunil Kamath, Joost Verburg

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Unicode support by Jim Park -- 08/20/2007

*/

#include "makensisw.h"
#include "resource.h"
#include "toolbar.h"
#include "noclib.h"

NTOOLTIP g_tip;
LRESULT CALLBACK TipHookProc(int nCode, WPARAM wParam, LPARAM lParam);

TCHAR g_mru_list[MRU_LIST_SIZE][MAX_PATH] = { _T(""), _T(""), _T(""), _T(""), _T("") };

extern NSCRIPTDATA g_sdata;
extern TCHAR *compressor_names[];

int SetArgv(const TCHAR *cmdLine, int *argc, TCHAR ***argv)
{
  const TCHAR *p;
  TCHAR *arg, *argSpace;
  int size, argSpaceSize, inquote, copy, slashes;

  size = 2;
  for (p = cmdLine; *p != _T('\0'); p++) {
    if ((*p == _T(' ')) || (*p == _T('\t'))) {
      size++;
      while ((*p == _T(' ')) || (*p == _T('\t'))) {
        p++;
      }
      if (*p == _T('\0')) {
        break;
      }
    }
  }

  argSpaceSize = size * sizeof(TCHAR *) + lstrlen(cmdLine) + 1;
  argSpace = (TCHAR *) GlobalAlloc(GMEM_FIXED, argSpaceSize);
  if (!argSpace)
    return 0;

  *argv = (TCHAR **) argSpace;
  argSpace += size * sizeof(TCHAR *);
  size--;

  p = cmdLine;
  for (*argc = 0; *argc < size; (*argc)++) {
    (*argv)[*argc] = arg = argSpace;
    while ((*p == _T(' ')) || (*p == _T('\t'))) {
      p++;
    }
    if (*p == _T('\0')) {
      break;
    }

    inquote = 0;
    slashes = 0;
    while (1) {
      copy = 1;
      while (*p == _T('\\')) {
        slashes++;
        p++;
      }
      if (*p == _T('"')) {
        if ((slashes & 1) == 0) {
          copy = 0;
          if ((inquote) && (p[1] == _T('"'))) {
            p++;
            copy = 1;
          }
          else {
            inquote = !inquote;
          }
        }
        slashes >>= 1;
      }

      while (slashes) {
        *arg = _T('\\');
        arg++;
        slashes--;
      }

      if ((*p == _T('\0')) || (!inquote && ((*p == _T(' ')) || (*p == _T('\t'))))) {
        break;
      }
      if (copy != 0) {
        *arg = *p;
        arg++;
      }
      p++;
    }
    *arg = _T('\0');
    argSpace = arg + 1;
  }
  (*argv)[*argc] = NULL;

  return argSpaceSize;
}

void SetTitle(HWND hwnd,TCHAR *substr) {
  TCHAR title[64];
  if (substr==NULL) wsprintf(title,_T("MakeNSISW"));
  else wsprintf(title,_T("MakeNSISW - %s"),substr); 
  SetWindowText(hwnd,title);
}

void SetBranding(HWND hwnd) {
  SetDlgItemText(hwnd, IDC_VERSION, g_sdata.branding);
}

void CopyToClipboard(HWND hwnd) {
  if (!hwnd||!OpenClipboard(hwnd)) return;
  int len=SendDlgItemMessage(hwnd,IDC_LOGWIN,WM_GETTEXTLENGTH,0,0);
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE,(len+1)*sizeof(TCHAR));
  if (!mem) { CloseClipboard(); return; }
  TCHAR *existing_text = (TCHAR *)GlobalLock(mem);
  if (!existing_text) { CloseClipboard(); return; }
  EmptyClipboard();
  existing_text[0]=0;
  GetDlgItemText(hwnd, IDC_LOGWIN, existing_text, len+1);
  GlobalUnlock(mem);
  SetClipboardData(CF_TEXT,mem);
  CloseClipboard();
}

void ClearLog(HWND hwnd) {
  SetDlgItemText(hwnd, IDC_LOGWIN, _T(""));
}

void LogMessage(HWND hwnd,const TCHAR *str) {
  SendDlgItemMessage(hwnd, IDC_LOGWIN, EM_SETSEL, g_sdata.logLength, g_sdata.logLength);
  g_sdata.logLength += lstrlen(str);
  SendDlgItemMessage(hwnd, IDC_LOGWIN, EM_REPLACESEL, 0, (LPARAM)str);
  SendDlgItemMessage(hwnd, IDC_LOGWIN, EM_SCROLLCARET, 0, 0);
}

void ErrorMessage(HWND hwnd,const TCHAR *str) {
  if (!str) return;
  TCHAR buf[1028];
  wsprintf(buf,_T("[Error] %s\r\n"),str);
  LogMessage(hwnd,buf);
}

// Altered by Darren Owen (DrO) on 1/10/2003
void Items(HWND hwnd, int on){
  UINT mf = (!on ? MF_GRAYED : MF_ENABLED);
  UINT nmf = (!on ? MF_ENABLED : MF_GRAYED);

  if(!on)
      g_sdata.focused_hwnd = GetFocus();
  // Altered by Darren Owen (DrO) on 6/10/2003
  else
    EnableWindow(GetDlgItem(hwnd,IDCANCEL),1);

  EnableWindow(GetDlgItem(hwnd,IDCANCEL),on);
  // Altered by Darren Owen (DrO) on 6/10/2003
  if((!g_sdata.retcode && on) || !on)
    EnableWindow(GetDlgItem(hwnd,IDC_TEST),on);
  EnableWindow(GetDlgItem(hwnd,IDC_RECOMPILE_TEST),on);

  EnableMenuItem(g_sdata.menu,IDM_SAVE,mf);
  // Altered by Darren Owen (DrO) on 6/10/2003
  if((!g_sdata.retcode && on) || !on)
    EnableMenuItem(g_sdata.menu,IDM_TEST,mf);
  EnableMenuItem(g_sdata.menu,IDM_EXIT,mf);
  EnableMenuItem(g_sdata.menu,IDM_LOADSCRIPT,mf);
  EnableMenuItem(g_sdata.menu,IDM_RECOMPILE,mf);
  EnableMenuItem(g_sdata.menu,IDM_COPY,mf);
  EnableMenuItem(g_sdata.menu,IDM_COPYSELECTED,mf);
  EnableMenuItem(g_sdata.menu,IDM_EDITSCRIPT,mf);
  EnableMenuItem(g_sdata.menu,IDM_CLEARLOG,mf);
  EnableMenuItem(g_sdata.menu,IDM_BROWSESCR,mf);
  EnableMenuItem(g_sdata.menu,IDM_RECOMPILE_TEST,mf);
  EnableMenuItem(g_sdata.menu,IDM_COMPRESSOR,mf);
  EnableMenuItem(g_sdata.menu,IDM_CANCEL,nmf);

  EnableToolBarButton(IDM_SAVE,on);
  // Altered by Darren Owen (DrO) on 6/10/2003
  if((!g_sdata.retcode && on) || !on)
    EnableToolBarButton(IDM_TEST,on);
  EnableToolBarButton(IDM_EXIT,on);
  EnableToolBarButton(IDM_LOADSCRIPT,on);
  EnableToolBarButton(IDM_RECOMPILE,on);
  EnableToolBarButton(IDM_COPY,on);
  EnableToolBarButton(IDM_EDITSCRIPT,on);
  EnableToolBarButton(IDM_CLEARLOG,on);
  EnableToolBarButton(IDM_BROWSESCR,on);
  EnableToolBarButton(IDM_RECOMPILE_TEST,on);
  EnableToolBarButton(IDM_COMPRESSOR,on);

  if(!on) {
    if (!IsWindowEnabled(g_sdata.focused_hwnd))
      SetFocus(GetDlgItem(hwnd,IDC_LOGWIN));
  }
  else
    SetFocus(g_sdata.focused_hwnd);
}

void SetCompressorStats()
{
  DWORD line_count, i;
  TCHAR buf[1024];
  bool found = false;

  line_count = SendDlgItemMessage(g_sdata.hwnd, IDC_LOGWIN, EM_GETLINECOUNT, 0, 0);
  for(i=0; i<line_count; i++) {
    *((LPWORD)buf) = sizeof(buf); 
    SendDlgItemMessage(g_sdata.hwnd, IDC_LOGWIN, EM_GETLINE, (WPARAM)i, (LPARAM)buf);
    if(found) {
      DWORD len = lstrlen(TOTAL_SIZE_COMPRESSOR_STAT);
      lstrcat(g_sdata.compressor_stats,buf);

      if(!lstrncmp(buf,TOTAL_SIZE_COMPRESSOR_STAT,len)) {
        break;
      }
    }
    else {
      DWORD len = lstrlen(EXE_HEADER_COMPRESSOR_STAT);
      if(!lstrncmp(buf,EXE_HEADER_COMPRESSOR_STAT,len)) {
        found = true;
        lstrcpy(g_sdata.compressor_stats,_T("\n\n"));
        lstrcat(g_sdata.compressor_stats,buf);
      }
    }
  }
}

void CompileNSISScript() {
  static TCHAR *s;
  DragAcceptFiles(g_sdata.hwnd,FALSE);
  ClearLog(g_sdata.hwnd);
  SetTitle(g_sdata.hwnd,NULL);
  if (lstrlen(g_sdata.script)==0) {
    LogMessage(g_sdata.hwnd,USAGE);
    EnableMenuItem(g_sdata.menu,IDM_RECOMPILE,MF_GRAYED);
    EnableMenuItem(g_sdata.menu,IDM_EDITSCRIPT,MF_GRAYED);
    EnableMenuItem(g_sdata.menu,IDM_TEST,MF_GRAYED);
    EnableMenuItem(g_sdata.menu,IDM_BROWSESCR,MF_GRAYED);
    // Added by Darren Owen (DrO) on 1/10/2003
    EnableMenuItem(g_sdata.menu,IDM_RECOMPILE_TEST,MF_GRAYED);

    EnableToolBarButton(IDM_RECOMPILE,FALSE);
    EnableToolBarButton(IDM_EDITSCRIPT,FALSE);
    EnableToolBarButton(IDM_TEST,FALSE);
    EnableToolBarButton(IDM_RECOMPILE_TEST,FALSE);
    EnableToolBarButton(IDM_BROWSESCR,FALSE);

    EnableWindow(GetDlgItem(g_sdata.hwnd,IDC_TEST),0);
    DragAcceptFiles(g_sdata.hwnd,TRUE);
    return;
  }
  if (!g_sdata.compile_command) {
    if (s) GlobalFree(s);
    TCHAR *symbols = BuildSymbols();
    
    TCHAR compressor[40];
    if(lstrlen(g_sdata.compressor_name)) {
      wsprintf(compressor,_T("/X\"SetCompressor /FINAL %s\""),g_sdata.compressor_name);
    }
    else {
      lstrcpy(compressor,_T(""));
    }

    TCHAR *args = (TCHAR *) GlobalLock(g_sdata.script_cmd_args);

    g_sdata.compile_command = (char *) GlobalAlloc(
      GPTR,
      /* makensis.exe        */ _countof(EXENAME)                 + /* space */ 1 +
      /* script path         */ lstrlen(g_sdata.script)           + /* space */ 1 +
      /* script cmd args     */ lstrlen(args)  + /* space */ 1 +
      /* defines /Dblah=...  */ lstrlen(symbols)                  + /* space */ 1 +
      /* /XSetCompressor...  */ lstrlen(compressor)               + /* space */ 1 +
      /* /NOTTIFYHWND + HWND */ _countof(_T("/NOTIFYHWND -4294967295")) + /* space */ 1
      );

    wsprintf(
      g_sdata.compile_command,
      _T("%s %s %s /NOTIFYHWND %d %s -- \"%s\""),
      EXENAME,
      compressor,
      symbols,
      g_sdata.hwnd,
      args,
      g_sdata.script
    );

    GlobalUnlock(g_sdata.script_cmd_args);
    GlobalFree(symbols);
  }
  GlobalFree(g_sdata.input_script);
  GlobalFree(g_sdata.output_exe);
  g_sdata.input_script = 0;
  g_sdata.output_exe = 0;
  g_sdata.warnings = 0;
  g_sdata.logLength = 0;
  // Disable buttons during compile
  DisableItems(g_sdata.hwnd);
  DWORD id;
  g_sdata.thread=CreateThread(NULL,0,MakeNSISProc,0,0,&id);
}

static bool InternalOpenRegSettingsKey(HKEY root, HKEY &key, bool create) {
  if (create) {
    if (RegCreateKey(root, REGKEY, &key) == ERROR_SUCCESS)
      return true;
  } else {
    if (RegOpenKeyEx(root, REGKEY, 0, KEY_READ, &key) == ERROR_SUCCESS)
      return true;
  }
  return false;
}

bool OpenRegSettingsKey(HKEY &hKey, bool create) {
  if (InternalOpenRegSettingsKey(REGSEC, hKey, create))
    return true;
  if (InternalOpenRegSettingsKey(REGSECDEF, hKey, create))
    return true;
  return false;
}

void RestoreWindowPos(HWND hwnd) {
  HKEY hKey;
  WINDOWPLACEMENT p;
  if (OpenRegSettingsKey(hKey)) {
    DWORD l = sizeof(p);
    DWORD t;
    if ((RegQueryValueEx(hKey,REGLOC,NULL,&t,(LPBYTE)&p,&l)==ERROR_SUCCESS)&&(t == REG_BINARY)&&(l==sizeof(p))) {
      int width, height;
      int windowWidth, windowHeight;

      width = GetSystemMetrics(SM_CXFULLSCREEN);
      height = GetSystemMetrics(SM_CYFULLSCREEN);
      height += GetSystemMetrics(SM_CYCAPTION);
      windowWidth = p.rcNormalPosition.right-p.rcNormalPosition.left;
      if(windowWidth > width) {
        p.rcNormalPosition.left = 0;
        p.rcNormalPosition.right = width;
      }
      else if(p.rcNormalPosition.right > width) {
        p.rcNormalPosition.left = width - windowWidth;
        p.rcNormalPosition.right = width;
      }
      else if(p.rcNormalPosition.left < 0) {
        p.rcNormalPosition.left = 0;
        p.rcNormalPosition.right = windowWidth;
      }

      windowHeight = p.rcNormalPosition.bottom-p.rcNormalPosition.top;
      if(windowHeight > height) {
        p.rcNormalPosition.top = 0;
        p.rcNormalPosition.bottom = height;
      }
      else if(p.rcNormalPosition.bottom > height) {
        p.rcNormalPosition.top = height - windowHeight;
        p.rcNormalPosition.bottom = height;
      }
      else if(p.rcNormalPosition.top < 0) {
        p.rcNormalPosition.top = 0;
        p.rcNormalPosition.bottom = windowHeight;
      }

      p.length = sizeof(p);
      SetWindowPlacement(hwnd, &p);
    }
    RegCloseKey(hKey);
  }
}

void SaveWindowPos(HWND hwnd) {
  HKEY hKey;
  WINDOWPLACEMENT p;
  p.length = sizeof(p);
  GetWindowPlacement(hwnd, &p);
  if (OpenRegSettingsKey(hKey, true)) {
    RegSetValueEx(hKey,REGLOC,0,REG_BINARY,(LPBYTE)&p,sizeof(p));
    RegCloseKey(hKey);
  }
}

void RestoreSymbols()
{
  g_sdata.symbols = LoadSymbolSet(NULL);
}

void SaveSymbols()
{
  SaveSymbolSet(NULL, g_sdata.symbols);
}

void DeleteSymbolSet(TCHAR *name)
{
  if(name) {
    HKEY hKey;
    if (OpenRegSettingsKey(hKey)) {
      TCHAR subkey[1024];
      wsprintf(subkey,_T("%s\\%s"),REGSYMSUBKEY,name);
      RegDeleteKey(hKey,subkey);
      RegCloseKey(hKey);
    }
  }
}

TCHAR** LoadSymbolSet(TCHAR *name)
{
  HKEY hKey;
  HKEY hSubKey;
  TCHAR **symbols = NULL;
  if (OpenRegSettingsKey(hKey)) {
    TCHAR subkey[1024];
    if(name) {
      wsprintf(subkey,_T("%s\\%s"),REGSYMSUBKEY,name);
    }
    else {
      lstrcpy(subkey,REGSYMSUBKEY);
    }
    if (RegCreateKey(hKey,subkey,&hSubKey) == ERROR_SUCCESS) {
      TCHAR buf[8];
      DWORD l;
      DWORD t;
      DWORD bufSize;
      DWORD i = 0;
      HGLOBAL hMem = NULL;

      while(TRUE) {
        l = 0;
        bufSize = sizeof(buf);
        if ((RegEnumValue(hSubKey,i, buf, &bufSize,NULL,&t,NULL,&l)==ERROR_SUCCESS)&&(t == REG_SZ)) {
          if(symbols) {
            GlobalUnlock(hMem);
            hMem = GlobalReAlloc(hMem, (i+2)*sizeof(TCHAR *), GMEM_MOVEABLE|GMEM_ZEROINIT);
            symbols = (TCHAR **)GlobalLock(hMem);
          }
          else {
            hMem = GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT, (i+2)*sizeof(TCHAR *));
            symbols = (TCHAR **)GlobalLock(hMem);
          }
          if(symbols) {
            l++;
            symbols[i] = (TCHAR *)GlobalAlloc(GPTR, l*sizeof(TCHAR));
            if (symbols[i]) {
              RegQueryValueEx(hSubKey,buf,NULL,&t,(unsigned char*)symbols[i],&l);
            }
            else {
              break;
            }
          }
          else {
            break;
          }
          i++;
          symbols[i] = NULL;
        }
        else {
          break;
        }
      }
      RegCloseKey(hSubKey);
    }
    RegCloseKey(hKey);
  }

  return symbols;
}

void SaveSymbolSet(TCHAR *name, TCHAR **symbols)
{
  HKEY hKey;
  HKEY hSubKey;
  int n = 0;
  if (OpenRegSettingsKey(hKey, true)) {
    TCHAR subkey[1024];
    if(name) {
      wsprintf(subkey,_T("%s\\%s"),REGSYMSUBKEY,name);
    }
    else {
      lstrcpy(subkey,REGSYMSUBKEY);
    }

    if (RegOpenKey(hKey,subkey,&hSubKey) == ERROR_SUCCESS) {
      TCHAR buf[8];
      DWORD l;
      while(TRUE) {
        l = sizeof(buf);
        if (RegEnumValue(hSubKey,0, buf, &l,NULL,NULL,NULL,NULL)==ERROR_SUCCESS) {
          RegDeleteValue(hSubKey,buf);
        }
        else {
          break;
        }
      }
      RegCloseKey(hSubKey);
    }
    if(symbols) {
      if (RegCreateKey(hKey,subkey,&hSubKey) == ERROR_SUCCESS) {
        TCHAR buf[8];
        n = 0;
        while(symbols[n]) {
          wsprintf(buf,_T("%d"),n);
          RegSetValueEx(hSubKey,buf,0,REG_SZ,(CONST BYTE *)symbols[n],(lstrlen(symbols[n])+1)*sizeof(TCHAR));
          n++;
        }
        RegCloseKey(hSubKey);
      }
    }
    RegCloseKey(hKey);
  }
}

void ResetObjects() {
  if (g_sdata.compile_command)
    GlobalFree(g_sdata.compile_command);

  g_sdata.warnings = FALSE;
  g_sdata.retcode = -1;
  g_sdata.thread = NULL;
  g_sdata.compile_command = NULL;
}

void ResetSymbols() {
  if(g_sdata.symbols) {
    HGLOBAL hMem;
    int i = 0;
    while(g_sdata.symbols[i]) {
      GlobalFree(g_sdata.symbols[i]);
      i++;
    }
    hMem = GlobalHandle(g_sdata.symbols);
    GlobalUnlock(hMem);
    GlobalFree(hMem);
    g_sdata.symbols = NULL;
  }
}

int InitBranding() {
  TCHAR *s;
  s = (TCHAR *)GlobalAlloc(GPTR,lstrlen(EXENAME)+10);
  wsprintf(s,_T("%s /version"),EXENAME);
  {
    STARTUPINFO si={sizeof(si),};
    SECURITY_ATTRIBUTES sa={sizeof(sa),};
    SECURITY_DESCRIPTOR sd={0,};
    PROCESS_INFORMATION pi={0,};
    HANDLE newstdout=0,read_stdout=0; 

    OSVERSIONINFO osv={sizeof(osv)};
    GetVersionEx(&osv);
    if (osv.dwPlatformId == VER_PLATFORM_WIN32_NT) {
      InitializeSecurityDescriptor(&sd,SECURITY_DESCRIPTOR_REVISION);
      SetSecurityDescriptorDacl(&sd,true,NULL,false);
      sa.lpSecurityDescriptor = &sd;
    }
    else sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = true;
    if (!CreatePipe(&read_stdout,&newstdout,&sa,0)) {
      return 0;
    }
    GetStartupInfo(&si);
    si.dwFlags = STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = newstdout;
    si.hStdError = newstdout;
    if (!CreateProcess(NULL,s,NULL,NULL,TRUE,CREATE_NEW_CONSOLE,NULL,NULL,&si,&pi)) {
      CloseHandle(newstdout);
      CloseHandle(read_stdout);
      return 0;
    }
    TCHAR szBuf[1024];
    DWORD dwRead = 1;
    if (WaitForSingleObject(pi.hProcess,10000)!=WAIT_OBJECT_0) {
      return 0;
    }
    ReadFile(read_stdout, szBuf, sizeof(szBuf)-1, &dwRead, NULL);
    szBuf[dwRead] = 0;
    if (lstrlen(szBuf)==0) return 0;
    g_sdata.branding = (TCHAR *)GlobalAlloc(GPTR,(lstrlen(szBuf)+6)*sizeof(TCHAR));
    wsprintf(g_sdata.branding,_T("NSIS %s"),szBuf);
    g_sdata.brandingv = (TCHAR *)GlobalAlloc(GPTR,(lstrlen(szBuf)+1)*sizeof(TCHAR));
    lstrcpy(g_sdata.brandingv,szBuf);
    GlobalFree(s);
  }
  return 1;
}

void InitTooltips(HWND h) {
  if (h == NULL)  return;
  my_memset(&g_tip,0,sizeof(NTOOLTIP));
  g_tip.tip_p = h;
  INITCOMMONCONTROLSEX icx;
  icx.dwSize  = sizeof(icx);
  icx.dwICC  = ICC_BAR_CLASSES;
  InitCommonControlsEx(&icx);
  DWORD dwStyle = WS_POPUP | WS_BORDER | TTS_ALWAYSTIP;
  DWORD dwExStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
  g_tip.tip = CreateWindowEx(dwExStyle,TOOLTIPS_CLASS,NULL,dwStyle,0,0,0,0,h,NULL,GetModuleHandle(NULL),NULL);
  if (!g_tip.tip) return;
  g_tip.hook = SetWindowsHookEx(WH_GETMESSAGE,TipHookProc,NULL, GetCurrentThreadId());
  AddTip(GetDlgItem(h,IDCANCEL),_T("Close MakeNSISW"));
  AddTip(GetDlgItem(h,IDC_TEST),_T("Test the installer generated by MakeNSISW"));
  AddToolBarTooltips();
}

void DestroyTooltips() {
  UnhookWindowsHookEx(g_tip.hook);
}

void AddTip(HWND hWnd,LPTSTR lpszToolTip) {
  TOOLINFO ti;
  ti.cbSize = sizeof(TOOLINFO);
  ti.uFlags = TTF_IDISHWND;
  ti.hwnd   = g_tip.tip_p;
  ti.uId = (UINT) hWnd;
  ti.lpszText = lpszToolTip;
  SendMessage(g_tip.tip, TTM_ADDTOOL, 0, (LPARAM) (LPTOOLINFO) &ti); 
}

LRESULT CALLBACK TipHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode < 0) return CallNextHookEx(g_tip.hook, nCode, wParam, lParam); 
  switch (((MSG*)lParam)->message) { 
    case WM_MOUSEMOVE:
      if (IsChild(g_tip.tip_p,((MSG*)lParam)->hwnd)) 
        SendMessage(g_tip.tip, TTM_RELAYEVENT, 0,lParam); 
      break; 
    default: 
      break; 
  } 
  return CallNextHookEx(g_tip.hook, nCode, wParam, lParam); 
}

void ShowDocs() {
  TCHAR pathf[MAX_PATH],*path;
  GetModuleFileName(NULL,pathf,sizeof(pathf));
  path=my_strrchr(pathf,_T('\\'));
  if(path!=NULL) *path=0;
  lstrcat(pathf,LOCALDOCS);
  if ((int)ShellExecute(g_sdata.hwnd,_T("open"),pathf,NULL,NULL,SW_SHOWNORMAL)<=32) 
  ShellExecuteA(g_sdata.hwnd,"open",DOCPATH,NULL,NULL,SW_SHOWNORMAL);
}

TCHAR* BuildSymbols()
{
  TCHAR *buf = NULL;

  if(g_sdata.symbols) {
    int i=0;
    while(g_sdata.symbols[i]) {
      if(buf) {
        TCHAR *buf3 = (TCHAR *)GlobalAlloc(GPTR,(lstrlen(buf)+lstrlen(g_sdata.symbols[i])+6)*sizeof(TCHAR));
        wsprintf(buf3,_T("%s \"/D%s\""),buf,g_sdata.symbols[i]);
        GlobalFree(buf);
        buf = buf3;
      }
      else {
        buf = (TCHAR *)GlobalAlloc(GPTR,(lstrlen(g_sdata.symbols[i])+5)*sizeof(TCHAR));
        wsprintf(buf,_T("\"/D%s\""),g_sdata.symbols[i]);
      }
      i++;
    }
  }
  else {
    buf = (TCHAR *)GlobalAlloc(GPTR, sizeof(TCHAR));
    buf[0] = _T('\0');
  }

  return buf;
}

BOOL PopMRUFile(TCHAR* fname)
{
  int i;

  for(i=0; i<MRU_LIST_SIZE; i++) {
    if(!lstrcmpi(g_mru_list[i], fname)) {
      break;
    }
  }

  if(i < MRU_LIST_SIZE) {
    int j;
    for(j = i; j < MRU_LIST_SIZE-1; j++) {
      lstrcpy(g_mru_list[j],g_mru_list[j+1]);
    }
    g_mru_list[MRU_LIST_SIZE-1][0]=_T('\0');
    return TRUE;
  }
  else {
    return FALSE;
  }
}

BOOL IsValidFile(TCHAR *fname)
{
  WIN32_FIND_DATA wfd;
  HANDLE h;

  h = FindFirstFile(fname,&wfd);
  if(h != INVALID_HANDLE_VALUE) {
    FindClose(h);
    return true;
  }
  return false;
}

void PushMRUFile(TCHAR* fname)
{
  int i;
  DWORD   rv;
  TCHAR*  file_part;
  TCHAR full_file_name[MAX_PATH+1];

  if(!fname || fname[0] == _T('\0') || fname[0] == _T('/') || fname[0] == _T('-')) {
    return;
  }

  my_memset(full_file_name,0,sizeof(full_file_name));
  rv = GetFullPathName(fname,_countof(full_file_name),full_file_name,&file_part);
  if (rv == 0) {
    return;
  }

  if(IsValidFile(full_file_name)) {
    PopMRUFile(full_file_name);
    for(i = MRU_LIST_SIZE - 2; i >= 0; i--) {
      lstrcpy(g_mru_list[i+1], g_mru_list[i]);
    }
    lstrcpy(g_mru_list[0],full_file_name);
    BuildMRUMenus();
  }
}

void BuildMRUMenus()
{
  HMENU hMenu = g_sdata.fileSubmenu;
  int i;
  MENUITEMINFO mii;
  TCHAR buf[MRU_DISPLAY_LENGTH + 5/*number*/ + 1/*null*/];
  TCHAR buf2[MRU_DISPLAY_LENGTH - 6];
  TCHAR buf3[MRU_DISPLAY_LENGTH + 1];
  int n;

  for(i = 0; i < MRU_LIST_SIZE; i++) {
    DeleteMenu(hMenu, IDM_MRU_FILE+i, MF_BYCOMMAND);
  }

  n = GetMenuItemCount(hMenu);

  for(i = 0; i < MRU_LIST_SIZE; i++) {
    if(g_mru_list[i][0]) {
      my_memset(buf,0,sizeof(buf));
      my_memset(&mii, 0, sizeof(mii));
      mii.cbSize = sizeof(mii);
      mii.fMask = MIIM_ID | MIIM_TYPE | MIIM_STATE;
      mii.wID = IDM_MRU_FILE+i;
      mii.fType = MFT_STRING;
      wsprintf(buf, _T("&%d "), i + 1);
      if(lstrlen(g_mru_list[i]) > MRU_DISPLAY_LENGTH) {
        TCHAR *p = my_strrchr(g_mru_list[i],_T('\\'));
        if(p) {
          p++;
          if(lstrlen(p) > MRU_DISPLAY_LENGTH - 7) {
            my_memset(buf2,0,sizeof(buf2));
            lstrcpyn(buf2,p,MRU_DISPLAY_LENGTH - 9);
            lstrcat(buf2,_T("..."));

            lstrcpyn(buf3,g_mru_list[i],4);
            lstrcat(buf,buf3);
            lstrcat(buf,_T("...\\"));
            lstrcat(buf,buf2);
          }
          else {
            lstrcpyn(buf3,g_mru_list[i],(MRU_DISPLAY_LENGTH - lstrlen(p) - 3));
            lstrcat(buf,buf3);
            lstrcat(buf,_T("...\\"));
            lstrcat(buf,p);
          }
        }
        else {
          lstrcpyn(buf3,g_mru_list[i],(MRU_DISPLAY_LENGTH-2));
          lstrcat(buf,buf3);
          lstrcat(buf,_T("..."));
        }
      }
      else {
        lstrcat(buf, g_mru_list[i]);
      }

      mii.dwTypeData = buf;
      mii.cch = lstrlen(buf)+1;
      mii.fState = MFS_ENABLED;
      InsertMenuItem(hMenu, n++, TRUE, &mii);

    }
    else {
      break;
    }
  }

  hMenu = g_sdata.toolsSubmenu;
  my_memset(&mii, 0, sizeof(mii));
  mii.cbSize = sizeof(mii);
  mii.fMask = MIIM_STATE;

  if(g_mru_list[0][0]) {
    mii.fState = MFS_ENABLED;
  }
  else {
    mii.fState = MFS_GRAYED;
  }

  SetMenuItemInfo(hMenu, IDM_CLEAR_MRU_LIST,FALSE,&mii);
}

void LoadMRUFile(int position)
{
  if (!g_sdata.thread && position >=0 && position < MRU_LIST_SIZE && g_mru_list[position][0]) {
    SetScript(g_mru_list[position]);
    if(IsValidFile(g_mru_list[position])) {
      PushMRUFile(g_mru_list[position]);
    }
    else {
      PopMRUFile(g_mru_list[position]);
      BuildMRUMenus();
    }
    ResetObjects();
    CompileNSISScript();
  }
}

void RestoreMRUList()
{
  HKEY hKey;
  HKEY hSubKey;
  int n = 0;
  int i;
  if (OpenRegSettingsKey(hKey)) {
    if (RegCreateKey(hKey,REGMRUSUBKEY,&hSubKey) == ERROR_SUCCESS) {
      TCHAR buf[8];
      DWORD l;
      for(int i=0; i<MRU_LIST_SIZE; i++) {
        wsprintf(buf,_T("%d"),i);
        l = sizeof(g_mru_list[n]);
        RegQueryValueEx(hSubKey,buf,NULL,NULL,(LPBYTE)g_mru_list[n],&l);
        if(g_mru_list[n][0] != _T('\0')) {
          n++;
        }
      }
      RegCloseKey(hSubKey);
    }
    RegCloseKey(hKey);
  }
  for(i = n; i < MRU_LIST_SIZE; i++) {
    g_mru_list[i][0] = _T('\0');
  }

  BuildMRUMenus();
}

void SaveMRUList()
{
  HKEY hKey;
  HKEY hSubKey;
  int i = 0;
  if (OpenRegSettingsKey(hKey, true)) {
    if (RegCreateKey(hKey,REGMRUSUBKEY,&hSubKey) == ERROR_SUCCESS) {
      TCHAR buf[8];
      for(i = 0; i < MRU_LIST_SIZE; i++) {
        wsprintf(buf,_T("%d"),i);
        // cbData must include the size of the terminating null character.
        RegSetValueEx(hSubKey,buf,0,REG_SZ,(const BYTE*)g_mru_list[i],(lstrlen(g_mru_list[i]))*sizeof(TCHAR));
      }
      RegCloseKey(hSubKey);
    }
    RegCloseKey(hKey);
  }
}

void ClearMRUList()
{
  int i;
  for(i=0; i<MRU_LIST_SIZE; i++) {
    g_mru_list[i][0] = _T('\0');
  }

  BuildMRUMenus();
}

void RestoreCompressor()
{
  HKEY hKey;
  NCOMPRESSOR v = COMPRESSOR_SCRIPT;
  if (OpenRegSettingsKey(hKey)) {
    TCHAR compressor_name[32];
    DWORD l = sizeof(compressor_name);
    DWORD t;

    if (RegQueryValueEx(hKey,REGCOMPRESSOR,NULL,&t,(LPBYTE)compressor_name,&l)==ERROR_SUCCESS) {
      int i;
      for(i=(int)COMPRESSOR_SCRIPT; i<= (int)COMPRESSOR_BEST; i++) {
        if(!lstrcmpi(compressor_names[i],compressor_name)) {
          v = (NCOMPRESSOR)i;
          break;
        }
      }
    }
    RegCloseKey(hKey);
  }
  g_sdata.default_compressor=v;
}

void SaveCompressor()
{
  HKEY hKey;
  int n = (int)COMPRESSOR_SCRIPT;
  NCOMPRESSOR v = g_sdata.default_compressor;

  if(v >= COMPRESSOR_SCRIPT && v <= COMPRESSOR_BEST) {
    n = (int)v;
  }

  if (OpenRegSettingsKey(hKey, true)) {
    // compressor_names, even if Unicode is saved as BYTE* data.
    RegSetValueEx(hKey,REGCOMPRESSOR,0,REG_SZ,(const BYTE*)compressor_names[n],
                  lstrlen(compressor_names[n])*sizeof(TCHAR));
    RegCloseKey(hKey);
  }
}

BOOL FileExists(TCHAR *fname)
{
  WIN32_FIND_DATA wfd;
  HANDLE h;

  h = FindFirstFile(fname,&wfd);
  if(h == INVALID_HANDLE_VALUE) {
    return false;
  }
  else {
    FindClose(h);
    return true;
  }
}

HMENU FindSubMenu(HMENU hMenu, UINT uId)
{
  MENUITEMINFO mii = {
    sizeof(MENUITEMINFO),
    MIIM_SUBMENU,
  };

  mii.hSubMenu = NULL;

  GetMenuItemInfo(hMenu, uId, FALSE, &mii);

  return mii.hSubMenu;
}
