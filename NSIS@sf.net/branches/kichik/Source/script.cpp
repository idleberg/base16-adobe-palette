#include <windows.h>
#include <stdio.h>
#include <shlobj.h>
#include "tokens.h"
#include "build.h"
#include "util.h"
#include "exedata.h"
#include "ResourceEditor.h"
#include "DialogTemplate.h"
#include "exehead/resource.h"

#ifndef FOF_NOERRORUI
#define FOF_NOERRORUI 0x0400
#endif


#define MAX_INCLUDEDEPTH 10
#define MAX_LINELENGTH 4096


static const char *usrvars="$0\0$1\0$2\0$3\0$4\0$5\0$6\0$7\0$8\0$9\0"
                             "$R0\0$R1\0$R2\0$R3\0$R4\0$R5\0$R6\0$R7\0$R8\0$R9\0"
                             "$CMDLINE\0$INSTDIR\0$OUTDIR\0$EXEDIR\0";


int CEXEBuild::process_script(FILE *fp, char *curfilename, int *lineptr)
{
  if (has_called_write_output)
  {
    ERROR_MSG("Error (process_script): write_output already called, can't continue\n");
    return PS_ERROR;
  }
  int ret=parseScript(fp,curfilename,lineptr,0);
  if (ret == PS_ENDIF) ERROR_MSG("!endif: stray !endif\n");
  if (IS_PS_ELSE(ret)) ERROR_MSG("!else: stray !else\n");
  if (m_linebuild.getlen())
  {
    ERROR_MSG("Error: invalid script: last line ended with \\\n");
    return PS_ERROR;
  }
  return ret;
}

#define PRINTHELP() { print_help(line.gettoken_str(0)); return PS_ERROR; }


int CEXEBuild::doParse(const char *str, FILE *fp, const char *curfilename, int *lineptr, int ignore)
{
  LineParser line;
  int res;

  while (*str == ' ' || *str == '\t') str++;

  // if ignoring, ignore all lines that don't begin with !.
  if (ignore && *str!='!') return PS_OK;
  
  if (m_linebuild.getlen()>1) m_linebuild.resize(m_linebuild.getlen()-2);

  m_linebuild.add(str,strlen(str)+1);

  // remove trailing slash and null
  if (str[0] && CharPrev(str,str+strlen(str))[0] == '\\') return PS_OK;

  res=line.parse((char*)m_linebuild.get());

  m_linebuild.resize(0);

  if (res)
  {
    if (res==-2) ERROR_MSG("Error: unterminated string parsing line at %s:%d\n",curfilename,*lineptr);
    else ERROR_MSG("Error: error parsing line (%s:%d)\n",curfilename,*lineptr);
    return PS_ERROR;
  }

parse_again:
  if (line.getnumtokens() < 1) return PS_OK;

  int np,op;
  int tkid=get_commandtoken(line.gettoken_str(0),&np,&op);
  if (tkid == -1)
  {
    char *p=line.gettoken_str(0);
    if (p[0] && p[strlen(p)-1]==':')
    {
      if (p[0] == '!' || (p[0] >= '0' && p[0] <= '9') || p[0] == '$' || p[0] == '-' || p[0] == '+')
      {
        ERROR_MSG("Invalid label: %s (labels cannot begin with !, $, -, +, or 0-9)\n",line.gettoken_str(0));
        return PS_ERROR;
      }
      if (add_label(line.gettoken_str(0))) return PS_ERROR;
      line.eattoken();
      goto parse_again;
    }
    ERROR_MSG("Invalid command: %s\n",line.gettoken_str(0));
    return PS_ERROR;
  }

  int v=line.getnumtokens()-(np+1);
  if (v < 0 || (op >= 0 && v > op)) // opt_parms is -1 for unlimited
  {
    ERROR_MSG("%s expects %d",line.gettoken_str(0),np);
    if (op < 0) ERROR_MSG("+");
    if (op > 0) ERROR_MSG("-%d",op);
    ERROR_MSG(" parameters, got %d.\n",line.getnumtokens()-1);
    PRINTHELP()
  }

  int is_elseif=0;

  if (!fp && (tkid == TOK_P_ELSE || tkid == TOK_P_IFNDEF || tkid == TOK_P_IFDEF))
  {
    ERROR_MSG("Error: !if/!else/!ifdef can only be specified in file mode, not in command/macro mode\n");
    return PS_ERROR;
  }

  if (tkid == TOK_P_ELSE) 
  {
    if (line.getnumtokens() == 1) return PS_ELSE;

    line.eattoken();

    int v=line.gettoken_enum(0,"ifdef\0ifndef\0");
    if (v < 0) PRINTHELP()
    if (line.getnumtokens() == 1) PRINTHELP()
    if (!v) tkid = TOK_P_IFDEF;
    else tkid = TOK_P_IFNDEF;
    is_elseif=1;
  }

  if (tkid == TOK_P_IFNDEF || tkid == TOK_P_IFDEF)
  {
    int istrue=0;
    if (!ignore || is_elseif)
    {
      int mod=0;
      int p;

       // pure left to right precedence. Not too powerful, but useful.
      for (p = 1; p < line.getnumtokens(); p ++)
      {         
        if (p & 1)
        {
          int new_s=!!definedlist.find(line.gettoken_str(p));
          if (tkid == TOK_P_IFNDEF) new_s=!new_s;

          if (mod == 0) istrue = istrue || new_s;
          else istrue = istrue && new_s;
        }
        else
        {
          mod=line.gettoken_enum(p,"|\0&\0||\0&&\0");
          if (mod == -1) PRINTHELP()
          mod &= 1;
        }
      }
      if (is_elseif)
      {
        if (istrue) return PS_ELSE_IF1;
        return PS_ELSE_IF0;
      }
    }

    int r;
    int hasexec=0;
    istrue=!istrue;

    for (;;)
    {
      r=parseScript(fp,curfilename,lineptr, ignore || hasexec || istrue);
      if (!istrue) hasexec=1;
      if (r == PS_ELSE_IF0) istrue=1;
      else if (r == PS_ELSE_IF1) istrue=0;
      else break;
    }

    if (r == PS_ELSE)
    {
      r=parseScript(fp,curfilename,lineptr, ignore || hasexec);
      if (IS_PS_ELSE(r))
      {
        ERROR_MSG("!else: stray !else\n");
        return PS_ERROR;
      }
    }
    if (r == PS_EOF)
    {
      ERROR_MSG("!ifdef: open at EOF - need !endif\n");
      return PS_ERROR;
    }
    if (r == PS_ERROR) return r;
    return PS_OK;
  }
  if (tkid == TOK_P_ENDIF) return PS_ENDIF;
  if (!ignore) 
  {
    int ret=doCommand(tkid,line,fp,curfilename,*lineptr);
    if (ret != PS_OK) return ret;
  }
  return PS_OK;
}

void CEXEBuild::ps_addtoline(const char *str, GrowBuf &linedata, StringList &hist)
{
    // convert $\r, $\n to their literals
    // preprocessor replace ${VAR} with whatever value
    // note that if VAR does not exist, ${VAR} will go through unmodified
  const char *in=str;
  while (*in)
  {
    int add=1;
    char *t;
    char c=*in;
    t=CharNext(in);

    if (t-in > 1) // handle multibyte chars (no escape)
    {
      linedata.add((void*)in,t-in);
      in=t;
      continue;
    }
    in=t;
    
    if (c == '$')
    { 
      if (in[0] == '\\')
      {
        if (in[1] == 'r')
        {
          in+=2;
          c='\r';
        }
        else if (in[1] == 'n')
        {
          in+=2;
          c='\n';
        }
      }
      else if (in[0] == '{')
      {
        char *s=strdup(in+1);
        char *t=s;
        while (*t)
        {
          if (*t == '}') break;
          t=CharNext(t);
        }
        if (*t && t!=s)
        {
          *t=0;
          t=definedlist.find(s);
          if (t && hist.find(s,0)<0)
          {
            in+=strlen(s)+2;
            add=0;
            hist.add(s,0);
            ps_addtoline(t,linedata,hist);
            hist.delbypos(hist.find(s,0));
          }
        }
        free(s);
      }
    }
    if (add) linedata.add((void*)&c,1);
  }
}

int CEXEBuild::parseScript(FILE *fp, const char *curfilename, int *lineptr, int ignore)
{
  char str[MAX_LINELENGTH];

  for (;;)
  {
    char *p=str;
    *p=0;
    fgets(str,MAX_LINELENGTH,fp);
    (*lineptr)++;
    if (feof(fp)&&!str[0]) break;

    // remove trailing whitespace
    while (*p) p++;
    if (p > str) p--;
    while (p >= str && (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t')) p--;
    *++p=0;

    StringList hist;
    GrowBuf linedata;
    ps_addtoline(str,linedata,hist);
    linedata.add((void*)"",1);
    int ret=doParse((char*)linedata.get(),fp,curfilename,lineptr,ignore);
    if (ret != PS_OK) return ret;
  }
  return PS_EOF;
}

int CEXEBuild::process_oneline(char *line, char *curfilename, int lineptr)
{
  StringList hist;
  GrowBuf linedata;
  ps_addtoline(line,linedata,hist);
  linedata.add((void*)"",1);
  return doParse((char*)linedata.get(),NULL,curfilename,&lineptr,0);
}

int CEXEBuild::process_jump(LineParser &line, int wt, int *offs)
{
  const char *s=line.gettoken_str(wt);
  int v;

  if (!stricmp(s,"0") || !stricmp(s,"")) *offs=0;
  else if ((v=line.gettoken_enum(wt,usrvars))>=0)
  {
    *offs=-v-1; // to jump to a user variable target, -variable_index-1 is stored.
  }
  else
  {
    if ((s[0] == '-' || s[0] == '+') && !atoi(s+1))
    {
      ERROR_MSG("Error: Goto targets beginning with '+' or '-' must be followed by nonzero integer (relative jump)\n");
      return 1;
    }
    if ((s[0] >= '0' && s[0] <= '9') || s[0] == '$' || s[0] == '!')
    {
      ERROR_MSG("Error: Goto targets cannot begin with 0-9, $, !\n");
      return 1;
    }
    *offs=ns_label.add(s,0);
  }
  return 0;
}

int CEXEBuild::doCommand(int which_token, LineParser &line, FILE *fp, const char *curfilename, int linecnt)
{
  static const char *rootkeys[2] = {
    "HKCR\0HKLM\0HKCU\0HKU\0HKCC\0HKDD\0HKPD\0",
    "HKEY_CLASSES_ROOT\0HKEY_LOCAL_MACHINE\0HKEY_CURRENT_USER\0HKEY_USERS\0HKEY_CURRENT_CONFIG\0HKEY_DYN_DATA\0HKEY_PERFORMANCE_DATA\0"
  };
  static HKEY rootkey_tab[] = {
    HKEY_CLASSES_ROOT,HKEY_LOCAL_MACHINE,HKEY_CURRENT_USER,HKEY_USERS,HKEY_CURRENT_CONFIG,HKEY_DYN_DATA,HKEY_PERFORMANCE_DATA
  };

  entry ent={0,};
  switch (which_token)
  {
    // macro shit
    ///////////////////////////////////////////////////////////////////////////////
    case TOK_P_MACRO:
      {
        if (!line.gettoken_str(1)[0]) PRINTHELP()
        char *t=(char *)m_macros.get();
        while (t && *t)
        {
          if (!stricmp(t,line.gettoken_str(1))) break;
          t+=strlen(t)+1;

          // advance over parameters
          while (*t) t+=strlen(t)+1;
          t++;

          // advance over data
          while (*t) t+=strlen(t)+1;
          if (t-(char *)m_macros.get() >= m_macros.getlen()-1)
            break;
          t++;
        }
        if (t && *t) 
        {
          ERROR_MSG("!macro: macro named \"%s\" already found!\n",line.gettoken_str(1));
          return PS_ERROR;
        }
        m_macros.add(line.gettoken_str(1),strlen(line.gettoken_str(1))+1);

        int pc;
        for (pc=2; pc < line.getnumtokens(); pc ++)
        {
          if (!line.gettoken_str(pc)[0])
          {
            ERROR_MSG("!macro: macro parameter %d is empty, not valid!\n",pc-1);
            return PS_ERROR;
          }
          int a;
          for (a=2; a < pc; a ++)
          {
            if (!stricmp(line.gettoken_str(pc),line.gettoken_str(a)))
            {
              ERROR_MSG("!macro: macro parameter named %s is used multiple times!\n",
                line.gettoken_str(pc));
              return PS_ERROR;
            }
          }
          m_macros.add(line.gettoken_str(pc),strlen(line.gettoken_str(pc))+1);
        }
        m_macros.add("",1);

        for (;;)
        {
          char str[MAX_LINELENGTH];
          char *p=str;
          str[0]=0;
          fgets(str,MAX_LINELENGTH,fp);
          if (feof(fp) || !str[0]) 
          {
            ERROR_MSG("!macro \"%s\": unterminated (no !macroend found in file)!\n",line.gettoken_str(1));
            return PS_ERROR;
          }
          // remove trailing whitespace
          while (*p) p++;
          if (p > str) p--;
          while (p >= str && (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t')) p--;
          *++p=0;
          LineParser l2;
          l2.parse(str);
          if (!stricmp(l2.gettoken_str(0),"!macroend")) break;
          if (str[0]) m_macros.add(str,strlen(str)+1);
          else m_macros.add(" ",2);
        }
        m_macros.add("",1);
      }
    return PS_OK;
    case TOK_P_MACROEND:
      ERROR_MSG("!macroend: no macro currently open.\n");
    return PS_ERROR;
    case TOK_P_INSERTMACRO:
      {
        if (!line.gettoken_str(1)[0]) PRINTHELP()
        char *t=(char *)m_macros.get();
        while (t && *t)
        {
          if (!stricmp(t,line.gettoken_str(1))) break;
          t+=strlen(t)+1;
          
          // advance over parms
          while (*t) t+=strlen(t)+1;
          t++;

          // advance over data
          while (*t) t+=strlen(t)+1;
          if (t-(char *)m_macros.get() >= m_macros.getlen()-1)
            break;
          t++;
        }
        SCRIPT_MSG("!insertmacro: %s\n",line.gettoken_str(1));
        if (!t || !*t) 
        {
          ERROR_MSG("!insertmacro: macro named \"%s\" not found!\n",line.gettoken_str(1));
          return PS_ERROR;
        }
        t+=strlen(t)+1;


        GrowBuf l_define_names;
        DefineList l_define_saves;
        int npr=0;
        // advance over parms
        while (*t) 
        {
          char *v;
          if (v=definedlist.find(t)) 
          {
            l_define_saves.add(t,v);
            definedlist.del(t);
          }
          l_define_names.add(t,strlen(t)+1);
          definedlist.add(t,line.gettoken_str(npr+2));

          npr++;
          t+=strlen(t)+1;
        }
        l_define_names.add("",1);
        t++;
        if (npr != line.getnumtokens()-2)
        {
          ERROR_MSG("!insertmacro: macro \"%s\" requires %d parameter(s), passed %d!\n",
            line.gettoken_str(1),npr,line.getnumtokens()-2);
          return PS_ERROR;
        }

        int lp=0;
        char str[1024];
        if (m_macro_entry.find(line.gettoken_str(1),0)>=0)
        {
          ERROR_MSG("!insertmacro: macro \"%s\" already being inserted!\n",line.gettoken_str(1));
          return PS_ERROR;
        }
        int npos=m_macro_entry.add(line.gettoken_str(1),0);

        wsprintf(str,"macro:%s",line.gettoken_str(1));
        while (*t)
        {
          lp++;
          if (strcmp(t," "))
          {
            int ret=process_oneline(t,str,lp);
            if (ret != PS_OK) 
            {
              ERROR_MSG("Error in macro %s on macroline %d\n",line.gettoken_str(1),lp);
              return ret;
            }
          }
          t+=strlen(t)+1;
        }
        m_macro_entry.delbypos(npos);
        {
          char *p=(char*)l_define_names.get();
          while (*p)
          {
            definedlist.del(p);
            char *v;
            if ((v=l_define_saves.find(p))) definedlist.add(p,v);
            p+=strlen(p);
          }
        }
        SCRIPT_MSG("!insertmacro: end of %s\n",line.gettoken_str(1));
      }

    return PS_OK;
    // header flags
    ///////////////////////////////////////////////////////////////////////////////
    case TOK_NAME:
      if (build_header.common.name_ptr >= 0)
      {
        warning("Name: specified multiple times, wasting space (%s:%d)",curfilename,linecnt);
      }
      build_header.common.name_ptr=add_string_main(line.gettoken_str(1),0);
#ifdef NSIS_CONFIG_UNINSTALL_SUPPORT
      build_uninst.common.name_ptr=add_string_uninst(line.gettoken_str(1),0);
#endif
      SCRIPT_MSG("Name: \"%s\"\n",line.gettoken_str(1));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_CAPTION:
      if (build_header.common.caption_ptr >= 0)
      {
        warning("Caption: specified multiple times, wasting space (%s:%d)",curfilename,linecnt);
      }
      build_header.common.caption_ptr=add_string_main(line.gettoken_str(1));
      SCRIPT_MSG("Caption: \"%s\"\n",line.gettoken_str(1));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_ICON:
      SCRIPT_MSG("Icon: \"%s\"\n",line.gettoken_str(1));
      try {
        build_compressor_set=true;
        CResourceEditor re(header_data_new, exeheader_size_new);
        if (replace_icon(&re, IDI_ICON2, line.gettoken_str(1))) {
          ERROR_MSG("Error: File doesn't exist or is an invalid icon file\n");
          return PS_ERROR;
        }
        free(header_data_new);
        header_data_new = re.Save((DWORD&)exeheader_size_new);
      }
      catch (exception& err) {
        ERROR_MSG("Error while replacing icon: %s\n", err.what());
        return PS_ERROR;
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
#ifdef NSIS_CONFIG_COMPONENTPAGE
    // Changed by Amir Szekely 24th July 2002
    case TOK_CHECKBITMAP:
      SCRIPT_MSG("CheckBitmap: \"%s\"\n",line.gettoken_str(1));
      try {
        build_compressor_set=true;
        CResourceEditor re(header_data_new, exeheader_size_new);
        if (update_bitmap(&re, IDB_BITMAP1, line.gettoken_str(1), 96, 16)) {
          ERROR_MSG("Error: File doesn't exist, is an invalid bitmap, or has the wrong size\n");
          return PS_ERROR;
        }
        free(header_data_new);
        header_data_new = re.Save((DWORD&)exeheader_size_new);
      }
      catch (exception& err) {
        ERROR_MSG("Error while replacing bitmap: %s\n", err.what());
        return PS_ERROR;
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
#else//NSIS_CONFIG_COMPONENTPAGE
    case TOK_ENABLEDBITMAP:
    case TOK_DISABLEDBITMAP:
      ERROR_MSG("Error: %s specified, NSIS_CONFIG_COMPONENTPAGE not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_CONFIG_COMPONENTPAGE
    case TOK_DIRTEXT:
      if (build_header.text_ptr >= 0 && line.gettoken_str(1)[0])
      {
        warning("DirText: specified multiple times, wasting space (%s:%d)",curfilename,linecnt);
      }
      build_header.text_ptr=add_string_main(line.gettoken_str(1),0);
      if (line.getnumtokens()>2) build_header.dirsubtext_ptr=add_string_main(line.gettoken_str(2),0);
      if (line.getnumtokens()>3) build_header.browse_ptr=add_string_main(line.gettoken_str(3),0);
      SCRIPT_MSG("DirText: \"%s\" \"%s\" \"%s\"\n",line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
#ifdef NSIS_CONFIG_COMPONENTPAGE
    case TOK_COMPTEXT:
      if (build_header.componenttext_ptr >= 0 && line.gettoken_str(1)[0])
      {
        warning("ComponentText: specified multiple times, wasting space (%s:%d)",curfilename,linecnt);
      }
      build_header.componenttext_ptr=add_string_main(line.gettoken_str(1),0);
      if (line.getnumtokens()>2) build_header.componentsubtext_ptr[0]=add_string_main(line.gettoken_str(2),0);
      if (line.getnumtokens()>3) build_header.componentsubtext_ptr[1]=add_string_main(line.gettoken_str(3),0);
      SCRIPT_MSG("ComponentText: \"%s\" \"%s\" \"%s\"\n",line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_INSTTYPE:
      {
        int x;
        if (!stricmp(line.gettoken_str(1),"/NOCUSTOM"))
        {
          build_header.no_custom_instmode_flag=1;
          SCRIPT_MSG("InstType: disabling custom install type\n");
        }
        else if (!stricmp(line.gettoken_str(1),"/COMPONENTSONLYONCUSTOM"))
        {
          build_header.no_custom_instmode_flag=2;
          SCRIPT_MSG("InstType: making components viewable only on custom install type\n");
        }
        else if (!strnicmp(line.gettoken_str(1),"/CUSTOMSTRING=",14))
        {
          build_header.custom_ptr=add_string_main(line.gettoken_str(1)+14,0);
          SCRIPT_MSG("InstType: setting custom text to: \"%s\"\n",line.gettoken_str(1)+14);
        }
        else if (line.gettoken_str(1)[0]=='/') PRINTHELP()
        else
        {
          for (x = 0; x < NSIS_MAX_INST_TYPES && build_header.install_types_ptr[x]>=0; x ++);
          if (x==NSIS_MAX_INST_TYPES)
          {
            ERROR_MSG("InstType: no more than %d install types allowed. %d specified\n",NSIS_MAX_INST_TYPES,NSIS_MAX_INST_TYPES+1);
            return PS_ERROR;
          }
          else 
          {
            build_header.install_types_ptr[x] = add_string_main(line.gettoken_str(1),0);
            SCRIPT_MSG("InstType: %d=\"%s\"\n",x+1,line.gettoken_str(1));
          }
        }
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
#else//NSIS_CONFIG_COMPONENTPAGE
    case TOK_COMPTEXT:
    case TOK_INSTTYPE:
      ERROR_MSG("Error: %s specified but NSIS_CONFIG_COMPONENTPAGE not defined\n",line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_CONFIG_COMPONENTPAGE
#ifdef NSIS_CONFIG_LICENSEPAGE
    case TOK_LICENSETEXT:
      if (build_header.licensetext_ptr >= 0)
      {
        warning("LicenseText: specified multiple times, wasting space (%s:%d)",curfilename,linecnt);
      }
      build_header.licensetext_ptr=add_string_main(line.gettoken_str(1),0);
      if (line.getnumtokens()>2) build_header.licensebutton_ptr=add_string_main(line.gettoken_str(2),0);
      SCRIPT_MSG("LicenseText: \"%s\" \"%s\"\n",line.gettoken_str(1),line.gettoken_str(2));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_LICENSEDATA:
      if (build_header.licensedata_ptr != -1)
      {
        warning("LicenseData: specified multiple times, wasting space (%s:%d)",curfilename,linecnt);
      }
#ifdef NSIS_CONFIG_SILENT_SUPPORT
      if (build_header.common.silent_install)
      {
        warning("LicenseData: SilentInstall enabled, wasting space (%s:%d)",curfilename,linecnt);
      }
#endif
      {
        FILE *fp;
        int datalen;
        fp=fopen(line.gettoken_str(1),"rb");
        if (!fp)
        {
          ERROR_MSG("LicenseData: open failed \"%s\"\n",line.gettoken_str(1));
          PRINTHELP()
        }
        fseek(fp,0,SEEK_END);
        datalen=ftell(fp);
        rewind(fp);
        char *data=(char*)malloc(datalen+1);
        if (fread(data,1,datalen,fp) != datalen) {
          ERROR_MSG("LicenseData: can't read file.\n");
          fclose(fp);
          return PS_ERROR;  
        }
        fclose(fp);
        data[datalen]=0;
        build_header.licensedata_ptr=add_string_main(data,0);
        SCRIPT_MSG("LicenseData: \"%s\"\n",line.gettoken_str(1));
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    // Added by Amir Szekely 30th July 2002
    case TOK_LICENSEBKCOLOR:
      {
        char *p = line.gettoken_str(1);
        int v=strtoul(p,&p,16);
        build_header.license_bg=((v&0xff)<<16)|(v&0xff00)|((v&0xff0000)>>16);
        SCRIPT_MSG("LicenseBkColor: %06X\n",v);
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
#else//!NSIS_CONFIG_LICENSEPAGE
    case TOK_LICENSETEXT:
    case TOK_LICENSEDATA:
    case TOK_LICENSEBKCOLOR:
      ERROR_MSG("Error: %s specified, NSIS_CONFIG_LICENSEPAGE not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_CONFIG_LICENSEPAGE
#ifdef NSIS_CONFIG_SILENT_SUPPORT
    case TOK_SILENTINST:
      build_header.common.silent_install=line.gettoken_enum(1,"normal\0silent\0silentlog\0");
      if (build_header.common.silent_install<0) PRINTHELP()
#ifndef NSIS_CONFIG_LOG
      if (build_header.common.silent_install == 2)
      {
        ERROR_MSG("SilentInstall: silentlog specified, no log support compiled in (use NSIS_CONFIG_LOG)\n");
        return PS_ERROR;
      }
#endif//NSIS_CONFIG_LOG
      SCRIPT_MSG("SilentInstall: %s\n",line.gettoken_str(1));
#ifdef NSIS_CONFIG_LICENSEPAGE
      if (build_header.common.silent_install && build_header.licensedata_ptr != -1)
      {
        warning("SilentInstall: LicenseData already specified. wasting space (%s:%d)",curfilename,linecnt);
      }
#endif//NSIS_CONFIG_LICENSEPAGE
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_SILENTUNINST:
#ifdef NSIS_CONFIG_UNINSTALL_SUPPORT
      build_uninst.common.silent_install=line.gettoken_enum(1,"normal\0silent\0");
      if (build_uninst.common.silent_install<0) PRINTHELP()
      SCRIPT_MSG("SilentUnInstall: %s\n",line.gettoken_str(1));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
#else
      ERROR_MSG("Error: %s specified, NSIS_CONFIG_UNINSTALL_SUPPORT not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif
#else//!NSIS_CONFIG_SILENT_SUPPORT
    case TOK_SILENTINST:
    case TOK_SILENTUNINST:
      ERROR_MSG("Error: %s specified, NSIS_CONFIG_SILENT_SUPPORT not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//NSIS_CONFIG_SILENT_SUPPORT
    case TOK_OUTFILE:
      strncpy(build_output_filename,line.gettoken_str(1),1024-1);
      SCRIPT_MSG("OutFile: \"%s\"\n",build_output_filename);
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_INSTDIR:
      if (build_header.install_directory_ptr >= 0)
      {
        warning("InstallDir: specified multiple times. wasting space (%s:%d)",curfilename,linecnt);
      }
      build_header.install_directory_ptr = add_string_main(line.gettoken_str(1));
      SCRIPT_MSG("InstallDir: \"%s\"\n",line.gettoken_str(1));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_INSTALLDIRREGKEY: // InstallDirRegKey
      {
        if (build_header.install_reg_key_ptr>= 0)
        {
          warning("InstallRegKey: specified multiple times, wasting space (%s:%d)",curfilename,linecnt);
        }
        int k=line.gettoken_enum(1,rootkeys[0]);
        if (k == -1) k=line.gettoken_enum(1,rootkeys[1]);
        if (k == -1) PRINTHELP()
        build_header.install_reg_rootkey=(int)rootkey_tab[k];
        build_header.install_reg_key_ptr = add_string_main(line.gettoken_str(2),0);
        if (line.gettoken_str(2)[0] == '\\') warning("%s: registry path name begins with \'\\\', may cause problems (%s:%d)",line.gettoken_str(0),curfilename,linecnt);
        build_header.install_reg_value_ptr = add_string_main(line.gettoken_str(3),0);
        SCRIPT_MSG("InstallRegKey: \"%s\\%s\\%s\"\n",line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3));
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_CRCCHECK:
      build_crcchk=line.gettoken_enum(1,"off\0on\0force\0");
      if (build_crcchk==-1) PRINTHELP()
      SCRIPT_MSG("CRCCheck: %s\n",line.gettoken_str(1));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_INSTPROGRESSFLAGS:
      {
        int x;
        build_header.common.progress_flags=0;
        for (x = 1; x < line.getnumtokens(); x ++)
        {
          if (!stricmp(line.gettoken_str(x),"smooth")) build_header.common.progress_flags|=1;
          else if (!stricmp(line.gettoken_str(x),"colored")) build_header.common.progress_flags|=2;
          else PRINTHELP()
        }
        SCRIPT_MSG("InstProgressFlags: %d (smooth=%d,colored=%d)\n",build_header.common.progress_flags,
          build_header.common.progress_flags&1,
          (build_header.common.progress_flags&2)>>1);
      }     
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_AUTOCLOSE:
      {
        int k=line.gettoken_enum(1,"false\0true\0");
        if (k == -1) PRINTHELP()
        build_header.common.misc_flags&=~1;
        build_header.common.misc_flags|=k;
        SCRIPT_MSG("AutoCloseWindow: %s\n",k?"true":"false");
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_WINDOWICON:
#ifdef NSIS_CONFIG_VISIBLE_SUPPORT
      // Changed by Amir Szekely 30th July 2002
      try {
        int k=line.gettoken_enum(1,"on\0off\0");
        if (k == -1) PRINTHELP();
        SCRIPT_MSG("WindowIcon: %s\n",line.gettoken_str(1));

        if (!k) return make_sure_not_in_secorfunc(line.gettoken_str(0));

        build_compressor_set=true;
        CResourceEditor re(header_data_new, exeheader_size_new);

#define REMOVE_ICON(id) { \
          BYTE* dlg = re.GetResource(RT_DIALOG, MAKEINTRESOURCE(id), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)); \
          if (!dlg) throw runtime_error(#id " doesn't exist!"); \
          CDialogTemplate dt(dlg); \
          free(dlg); \
          dt.RemoveItem(IDC_ULICON); \
          DialogItemTemplate* text = dt.GetItem(IDC_INTROTEXT); \
          DialogItemTemplate* prog1 = dt.GetItem(IDC_PROGRESS1); \
          DialogItemTemplate* prog2 = dt.GetItem(IDC_PROGRESS2); \
          if (text) { \
            text->sWidth += text->sX; \
            text->sX = 0; \
          } \
          if (prog1) { \
            prog1->sWidth += prog1->sX; \
            prog1->sX = 0; \
          } \
          if (prog2) { \
            prog2->sWidth += prog2->sX; \
            prog2->sX = 0; \
          } \
           \
          DWORD dwSize; \
          dlg = dt.Save(dwSize); \
          re.UpdateResource(RT_DIALOG, MAKEINTRESOURCE(id), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), dlg, dwSize); \
          free(dlg); \
        }

#ifdef NSIS_CONFIG_LICENSEPAGE
        REMOVE_ICON(IDD_LICENSE);
#endif
        REMOVE_ICON(IDD_DIR);
#ifdef NSIS_CONFIG_COMPONENTPAGE
        REMOVE_ICON(IDD_SELCOM);
#endif
        REMOVE_ICON(IDD_INSTFILES);
#ifdef NSIS_CONFIG_UNINSTALL_SUPPORT
        REMOVE_ICON(IDD_UNINST);
#endif
#ifdef NSIS_CONFIG_CRC_SUPPORT
        REMOVE_ICON(IDD_VERIFY);
#endif

        free(header_data_new);
        header_data_new = re.Save((DWORD&)exeheader_size_new);
      }
      catch (exception& err) {
        ERROR_MSG("Error while changing font: %s\n", err.what());
        return PS_ERROR;
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
#else
    ERROR_MSG("Error: %s specified, NSIS_CONFIG_VISIBLE_SUPPORT not defined.\n",line.gettoken_str(0));
    return PS_ERROR;
#endif // NSIS_CONFIG_VISIBLE_SUPPORT
    case TOK_SHOWDETAILSUNINST:
#ifndef NSIS_CONFIG_UNINSTALL_SUPPORT
      ERROR_MSG("Error: ShowUninstDetails specified but NSIS_CONFIG_UNINSTALL_SUPPORT not defined\n");
      return PS_ERROR;
#endif
    case TOK_SHOWDETAILS:
      {
        int k=line.gettoken_enum(1,"hide\0show\0nevershow\0");
        if (k == -1) PRINTHELP()
#ifdef NSIS_CONFIG_UNINSTALL_SUPPORT
        if (which_token == TOK_SHOWDETAILSUNINST) build_uninst.common.show_details=k;
        else 
#endif
          build_header.common.show_details=k;
        SCRIPT_MSG("%s: %s\n",line.gettoken_str(0),line.gettoken_str(1));
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_DIRSHOW:
      {
        int k=line.gettoken_enum(1,"show\0hide\0");
        if (k == -1) PRINTHELP()
        build_header.common.misc_flags&=~2;
        build_header.common.misc_flags|=(k<<1);
        SCRIPT_MSG("DirShow: %s\n",k?"hide":"show");
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_ROOTDIRINST:
      {
        int k=line.gettoken_enum(1,"true\0false\0");
        if (k == -1) PRINTHELP()
        build_header.common.misc_flags&=~8;
        build_header.common.misc_flags|=(k<<3);
        SCRIPT_MSG("AllowRootDirInstall: %s\n",k?"false":"true");
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_BGGRADIENT:
#ifndef NSIS_SUPPORT_BGBG
      ERROR_MSG("Error: BGGradient specified but NSIS_SUPPORT_BGBG not defined\n");
      return PS_ERROR;
#else//NSIS_SUPPORT_BGBG
      if (line.getnumtokens()==1)
      {
        SCRIPT_MSG("BGGradient: default colors\n");
        build_header.common.bg_color1=0;
        build_header.common.bg_color2=RGB(0,0,255);
      }
      else if (!stricmp(line.gettoken_str(1),"off"))
      {
        build_header.common.bg_color1=build_header.common.bg_color2=-1;
        SCRIPT_MSG("BGGradient: off\n");
        if (line.getnumtokens()>2) PRINTHELP()
      }
      else
      {
        char *p = line.gettoken_str(1);
        int v1,v2,v3=-1;
        v1=strtoul(p,&p,16);
        build_header.common.bg_color1=((v1&0xff)<<16)|(v1&0xff00)|((v1&0xff0000)>>16);
        p=line.gettoken_str(2);
        v2=strtoul(p,&p,16);
        build_header.common.bg_color2=((v2&0xff)<<16)|(v2&0xff00)|((v2&0xff0000)>>16);

        p=line.gettoken_str(3);
        if (*p)
        {
          if (!stricmp(p,"notext")) build_header.common.bg_textcolor=-1;
          else
          {
            v3=strtoul(p,&p,16);
            build_header.common.bg_textcolor=((v3&0xff)<<16)|(v3&0xff00)|((v3&0xff0000)>>16);
          }
        }
        
        SCRIPT_MSG("BGGradient: %06X->%06X (text=%d)\n",v1,v2,v3);
      }

#ifdef NSIS_CONFIG_UNINSTALL_SUPPORT
      build_uninst.common.bg_color1=build_header.common.bg_color1;
      build_uninst.common.bg_color2=build_header.common.bg_color2;
      build_uninst.common.bg_textcolor=build_header.common.bg_textcolor;
#endif//NSIS_CONFIG_UNINSTALL_SUPPORT
#endif//NSIS_SUPPORT_BGBG
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_INSTCOLORS:
      {
        char *p = line.gettoken_str(1);
        if (p[0]=='/')
        {
          if (stricmp(p,"/windows") || line.getnumtokens()!=2) PRINTHELP()
          build_header.common.lb_fg=build_header.common.lb_bg=-1;
          SCRIPT_MSG("InstallColors: windows default colors\n");
        }
        else
        {
          int v1,v2;
          if (line.getnumtokens()!=3) PRINTHELP()
          v1=strtoul(p,&p,16);
          build_header.common.lb_fg=((v1&0xff)<<16)|(v1&0xff00)|((v1&0xff0000)>>16);
          p=line.gettoken_str(2);
          v2=strtoul(p,&p,16);
          build_header.common.lb_bg=((v2&0xff)<<16)|(v2&0xff00)|((v2&0xff0000)>>16);
          SCRIPT_MSG("InstallColors: fg=%06X bg=%06X\n",v1,v2);
        }

#ifdef NSIS_CONFIG_UNINSTALL_SUPPORT
        build_uninst.common.lb_fg=build_header.common.lb_fg;
        build_uninst.common.lb_bg=build_header.common.lb_bg;
#endif
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    // Added by Amir Szekely 7th July 2002
	  case TOK_XPSTYLE:
      try {
        int k=line.gettoken_enum(1,"on\0off\0");
        if (k == -1) PRINTHELP()
        SCRIPT_MSG("XPStyle: %s\n", line.gettoken_str(1));
        if (k == 0) {
          build_compressor_set=true;
          CResourceEditor re(header_data_new, exeheader_size_new);
          char* szXPManifest = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\"><assemblyIdentity version=\"1.0.0.0\" processorArchitecture=\"X86\" name=\"Nullsoft.NSIS.exehead\" type=\"win32\"/><description>Nullsoft Install System.</description><dependency><dependentAssembly><assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\" version=\"6.0.0.0\" processorArchitecture=\"X86\" publicKeyToken=\"6595b64144ccf1df\" language=\"*\" /></dependentAssembly></dependency></assembly>";
          re.UpdateResource(MAKEINTRESOURCE(24), MAKEINTRESOURCE(1), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (unsigned char*)szXPManifest, lstrlen(szXPManifest));
          free(header_data_new);
          header_data_new = re.Save((DWORD&)exeheader_size_new);
        }
      }
      catch (exception& err) {
        ERROR_MSG("Error while adding XP style: %s\n", err.what());
        return PS_ERROR;
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    // Added by Amir Szekely 28th July 2002
#ifdef NSIS_CONFIG_VISIBLE_SUPPORT
	  case TOK_CHANGEUI:
	  try {
        HINSTANCE hUIFile = LoadLibraryEx(line.gettoken_str(1), 0, LOAD_LIBRARY_AS_DATAFILE);
        if (!hUIFile) {
          ERROR_MSG("Error: Can't find \"%s\"!\n", line.gettoken_str(1));
          return PS_ERROR;
        }
        HRSRC hUIRes = FindResource(hUIFile, MAKEINTRESOURCE(IDD_INST), RT_DIALOG);
        if (!hUIRes) {
          ERROR_MSG("Error: \"%s\" doesn't contain a dialog named IDD_INST (%u)!\n", line.gettoken_str(1), IDD_INST);
          return PS_ERROR;
        }
        HGLOBAL hUIMem = LoadResource(hUIFile, hUIRes);
        if (!hUIMem) {
          ERROR_MSG("Error: Can't load a dialog from \"%s\"!\n", line.gettoken_str(1));
          return PS_ERROR;
        }
        BYTE* pbUIData = (BYTE*)LockResource(hUIMem);
        if (!pbUIData) {
          ERROR_MSG("Error: Can't lock resource from \"%s\"!\n", line.gettoken_str(1));
          return PS_ERROR;
        }

        CDialogTemplate UIDlg(pbUIData);

        // Search for required items
        #define SEARCH(x) if (!UIDlg.GetItem(IDC_BACK)) {ERROR_MSG("Error: Can't find %s (%u) in the custom UI!\n", #x, x);return PS_ERROR;}
        SEARCH(IDC_BACK);
        SEARCH(IDC_CHILDRECT);
        SEARCH(IDC_VERSTR);
        SEARCH(IDOK);
        SEARCH(IDCANCEL);

        // Search for bitmap holder (default for SetBrandingImage)
        DialogItemTemplate* dlgItem = 0;
        int i = 0;
        while (dlgItem = UIDlg.GetItemByIdx(i)) {
          if (IS_INTRESOURCE(dlgItem->szClass)) {
            if (dlgItem->szClass == MAKEINTRESOURCE(0x0082)) {
              if ((dlgItem->dwStyle & SS_BITMAP) == SS_BITMAP) {
                branding_image_found = true;
                branding_image_id = dlgItem->wId;
                break;
              }
            }
          }
          i++;
        }
        
        build_compressor_set=true;
        CResourceEditor re(header_data_new, exeheader_size_new);
        re.UpdateResource(RT_DIALOG, IDD_INST, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), pbUIData, UIDlg.GetSize());
        free(header_data_new);
        header_data_new = re.Save((DWORD&)exeheader_size_new);

        SCRIPT_MSG("ChangeUI: %s%s\n", line.gettoken_str(1), branding_image_found?" (branding image holder found)":"");
      }
      catch (exception& err) {
        ERROR_MSG("Error while changing UI: %s\n", err.what());
        return PS_ERROR;
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
#else
    ERROR_MSG("Error: %s specified, NSIS_CONFIG_VISIBLE_SUPPORT not defined.\n",line.gettoken_str(0));
    return PS_ERROR;
#endif// NSIS_CONFIG_VISIBLE_SUPPORT
    // Added by Amir Szekely 21st July 2002
#ifdef NSIS_CONFIG_VISIBLE_SUPPORT
	  case TOK_ADDBRANDINGIMAGE:
	  try {
        int k=line.gettoken_enum(1,"top\0left\0");
        int wh=line.gettoken_int(2);
        if (k == -1) PRINTHELP()
        
        CResourceEditor re(header_data_new, exeheader_size_new);
        BYTE* dlg = re.GetResource(RT_DIALOG, MAKEINTRESOURCE(IDD_INST), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));

        CDialogTemplate dt(dlg);
        delete [] dlg;

        DialogItemTemplate *childRect = dt.GetItem(IDC_CHILDRECT);
        DialogItemTemplate brandingCtl = {0,};

        brandingCtl.dwStyle = SS_BITMAP | WS_CHILD | WS_VISIBLE;
        brandingCtl.sX = childRect->sX;
        brandingCtl.sY = childRect->sY;
        brandingCtl.szClass = MAKEINTRESOURCE(0x0082);
        brandingCtl.szTitle = "";
        brandingCtl.wId = IDC_BRANDIMAGE;

        brandingCtl.sHeight = wh;
        brandingCtl.sWidth = wh;
        dt.PixelsToDlgUnits(brandingCtl.sWidth, brandingCtl.sHeight);
        if (k) {
          // Left
          dt.MoveAllAndResize(brandingCtl.sWidth + childRect->sX, 0);

          DialogItemTemplate *okButton = dt.GetItem(IDOK);
          brandingCtl.sHeight = okButton->sY + okButton->sHeight - childRect->sY;
        }
        else {
          // Top
          dt.MoveAllAndResize(0, brandingCtl.sHeight + childRect->sY);

          brandingCtl.sWidth = childRect->sWidth;
        }

        dt.AddItem(brandingCtl);

        DWORD dwDlgSize;
        dlg = dt.Save(dwDlgSize);

        re.UpdateResource(RT_DIALOG, IDD_INST, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), dlg, dwDlgSize);

        delete [] dlg;

        free(header_data_new);
        header_data_new = re.Save((DWORD&)exeheader_size_new);

        dt.DlgUnitsToPixels(brandingCtl.sWidth, brandingCtl.sHeight);
        SCRIPT_MSG("AddBrandingImage: %s %ux%u\n", line.gettoken_str(1), brandingCtl.sWidth, brandingCtl.sHeight);

        branding_image_found = true;
        branding_image_id = IDC_BRANDIMAGE;
      }
      catch (exception& err) {
        ERROR_MSG("Error while adding image branding support: %s\n", err.what());
        return PS_ERROR;
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
#else
    ERROR_MSG("Error: %s specified, NSIS_CONFIG_VISIBLE_SUPPORT not defined.\n",line.gettoken_str(0));
    return PS_ERROR;
#endif// NSIS_CONFIG_VISIBLE_SUPPORT
#ifdef NSIS_CONFIG_VISIBLE_SUPPORT
	  case TOK_SETFONT:
      SCRIPT_MSG("SetFont: \"%s\" %s\n", line.gettoken_str(1), line.gettoken_str(2));
      try {
        build_compressor_set=true;
        CResourceEditor re(header_data_new, exeheader_size_new);

#define SET_FONT(id) { \
          BYTE* dlg = re.GetResource(RT_DIALOG, MAKEINTRESOURCE(id), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)); \
          if (!dlg) throw runtime_error(#id " doesn't exist!"); \
          CDialogTemplate td(dlg); \
          free(dlg); \
          td.SetFont(line.gettoken_str(1), line.gettoken_int(2)); \
          DWORD dwSize; \
          dlg = td.Save(dwSize); \
          re.UpdateResource(RT_DIALOG, MAKEINTRESOURCE(id), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), dlg, dwSize); \
          free(dlg); \
        }

#ifdef NSIS_CONFIG_LICENSEPAGE
        SET_FONT(IDD_LICENSE);
#endif
        SET_FONT(IDD_DIR);
#ifdef NSIS_CONFIG_COMPONENTPAGE
        SET_FONT(IDD_SELCOM);
#endif
        SET_FONT(IDD_INST);
        SET_FONT(IDD_INSTFILES);
#ifdef NSIS_CONFIG_UNINSTALL_SUPPORT
        SET_FONT(IDD_UNINST);
#endif
#ifdef NSIS_CONFIG_CRC_SUPPORT
        SET_FONT(IDD_VERIFY);
#endif

        free(header_data_new);
        header_data_new = re.Save((DWORD&)exeheader_size_new);
      }
      catch (exception& err) {
        ERROR_MSG("Error while changing font: %s\n", err.what());
        return PS_ERROR;
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
#else
    ERROR_MSG("Error: %s specified, NSIS_CONFIG_VISIBLE_SUPPORT not defined.\n",line.gettoken_str(0));
    return PS_ERROR;
#endif// NSIS_CONFIG_VISIBLE_SUPPORT
    // Added by Amir Szekely 31st July 2002
    // Ability to change compression methods from within the script
    case TOK_SETCOMPRESSOR:
      {
        if (build_compressor_set) {
          ERROR_MSG("Error: can't change compressor after data already got compressed or header already changed!\n");
          return PS_ERROR;
        }
        int k=line.gettoken_enum(1,"zlib\0bzip2\0");
        switch (k) {
        	case 0:
            // Default is zlib...
            break;
          case 1:
            compressor=&bzip2_compressor;
            free(header_data_new);
            header_data_new=(unsigned char*)malloc(bzip2_exeheader_size);
            exeheader_size_new=bzip2_exeheader_size;
            exeheader_size=bzip2_exeheader_size;

            if (!header_data_new) 
            {
              ERROR_MSG("Internal compiler error #12345: malloc(%d) failed\n",exeheader_size_new);
              extern void quit(); quit();
            }

            memcpy(header_data_new,bzip2_header_data,bzip2_exeheader_size);
#ifdef NSIS_BZIP2_COMPRESS_WHOLE
            build_compress_whole=true;
#else
            build_compress_whole=false;
#endif
            break;
          default:
            PRINTHELP();
        }
        SCRIPT_MSG("SetCompressor: %s\n", line.gettoken_str(1));
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));

    // preprocessor-ish (ifdef/ifndef/else/endif are handled one step out from here)
    ///////////////////////////////////////////////////////////////////////////////
    case TOK_P_DEFINE:
      if (definedlist.add(line.gettoken_str(1),line.gettoken_str(2)))
      {
        ERROR_MSG("!define: \"%s\" already defined!\n",line.gettoken_str(1));
        return PS_ERROR;
      }
      SCRIPT_MSG("!define: \"%s\"=\"%s\"\n",line.gettoken_str(1),line.gettoken_str(2));
    return PS_OK;
    case TOK_P_UNDEF:
      if (definedlist.del(line.gettoken_str(1)))
      {
        ERROR_MSG("!undef: \"%s\" not defined!\n",line.gettoken_str(1));
        return PS_ERROR;
      }
      SCRIPT_MSG("!undef: \"%s\"\n",line.gettoken_str(1));
    return PS_OK;
    case TOK_P_PACKEXEHEADER:
      strncpy(build_packname,line.gettoken_str(1),sizeof(build_packname)-1);
      strncpy(build_packcmd,line.gettoken_str(2),sizeof(build_packcmd)-1);
      SCRIPT_MSG("!packhdr: filename=\"%s\", command=\"%s\"\n",
        build_packname, build_packcmd);
    return PS_OK;
    case TOK_P_SYSTEMEXEC:
      {
        char *exec=line.gettoken_str(1);
        int comp=line.gettoken_enum(2,"<\0>\0<>\0=\0ignore\0");
        if (comp == -1 && line.getnumtokens() == 3) comp=4;
        if (comp == -1) PRINTHELP()
        int success=0;
        int cmpv=line.gettoken_int(3,&success);
        if (!success && comp != 4) PRINTHELP()
        SCRIPT_MSG("!system: \"%s\"\n",exec);
        int ret=system(exec);
        if (comp == 0 && ret < cmpv);
        else if (comp == 1 && ret > cmpv);
        else if (comp == 2 && ret != cmpv);
        else if (comp == 3 && ret == cmpv);
        else if (comp == 4);
        else
        {
          ERROR_MSG("!system: returned %d, aborting\n",ret);
          return PS_ERROR;
        }
        SCRIPT_MSG("!system: returned %d\n",ret);
      }
    return PS_OK;
    case TOK_P_INCLUDE:
      {
        char *f=line.gettoken_str(1);
        SCRIPT_MSG("!include: \"%s\"\n",f);
        FILE *incfp=fopen(f,"rt");
        if (!incfp)
        {
          ERROR_MSG("!include: could not open file: \"%s\"\n",f);
          return PS_ERROR;
        }
        static int depth;
        if (depth >= MAX_INCLUDEDEPTH)
        {
          ERROR_MSG("parseScript: too many levels of includes (%d max).\n",MAX_INCLUDEDEPTH);
          return PS_ERROR;
        }
        depth++;
        int lc=0;
        int r=parseScript(incfp,f,&lc,0);
        depth--;
        fclose(incfp);
        if (r != PS_EOF && r != PS_OK)
        {
          if (r == PS_ENDIF) ERROR_MSG("!endif: stray !endif\n");
          if (IS_PS_ELSE(r)) ERROR_MSG("!else: stray !else\n");
          ERROR_MSG("!include: error in script: \"%s\" on line %d\n",f,lc);
          return PS_ERROR;
        }
        SCRIPT_MSG("!include: closed: \"%s\"\n",f);
      }
    return PS_OK;
    case TOK_P_CD:
      if (!line.gettoken_str(1)[0] || !SetCurrentDirectory(line.gettoken_str(1)))
      {
        ERROR_MSG("!cd: error changing to: \"%s\"\n",line.gettoken_str(1));
        return PS_ERROR;
      }
    return PS_OK;
    case TOK_P_ERROR:
      ERROR_MSG("!error: %s\n",line.gettoken_str(1));
    return PS_ERROR;
    case TOK_P_WARNING:
      warning("!warning: %s (%s:%d)\n",line.gettoken_str(1),curfilename,linecnt);
    return PS_OK;
    // Added by Amir Szekely 23rd July 2002
    case TOK_P_ECHO:
      SCRIPT_MSG("%s (%s:%d)\n", line.gettoken_str(1),curfilename,linecnt);
    return PS_OK;

    // Added by Amir Szekely 23rd July 2002
    case TOK_P_VERBOSE:
    {
      extern int g_display_errors;
      int v=line.gettoken_int(1);
      display_script=v>3;
      display_info=v>2;
      display_warnings=v>1;
      display_errors=v>0;
      g_display_errors=display_errors;
    }
    return PS_OK;

    case TOK_UNINSTALLEXENAME: PRINTHELP()
    
    
#ifdef NSIS_CONFIG_UNINSTALL_SUPPORT
    case TOK_UNINSTCAPTION:
      if (build_uninst.common.caption_ptr >= 0)
      {
        warning("UninstCaption: specified multiple times, wasting space (%s:%d)",curfilename,linecnt);
      }
      build_uninst.common.caption_ptr=add_string_uninst(line.gettoken_str(1));
      SCRIPT_MSG("UninstCaption: \"%s\"\n",line.gettoken_str(1));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_UNINSTICON:
      SCRIPT_MSG("UninstallIcon: \"%s\"\n",line.gettoken_str(1));
      try {
        free(m_unicon_data);
        m_unicon_data = generate_uninstall_icon_data(line.gettoken_str(1));
        if (!m_unicon_data) {
          ERROR_MSG("Error: File doesn't exist or is an invalid icon file\n");
          return PS_ERROR;
        }
      }
      catch (exception& err) {
        ERROR_MSG("Error while replacing icon: %s\n", err.what());
        return PS_ERROR;
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_UNINSTTEXT:
      if (build_uninst.uninstalltext_ptr >= 0)
      {
        warning("UninstallText: specified multiple times, wasting space (%s:%d)",curfilename,linecnt);
      }
      build_uninst.uninstalltext_ptr=add_string_uninst(line.gettoken_str(1),0);
      if (line.getnumtokens()>2) build_uninst.uninstalltext2_ptr=add_string_uninst(line.gettoken_str(2),0);
      SCRIPT_MSG("UninstallText: \"%s\" \"%s\"\n",line.gettoken_str(1),line.gettoken_str(2));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_UNINSTSUBCAPTION:
      {
        int s;
        int w=line.gettoken_int(1,&s);
        if (!s || w < 0 || w > 2) PRINTHELP()
        build_uninst.common.subcaption_ptrs[w]=add_string_uninst(line.gettoken_str(2));
        SCRIPT_MSG("UninstSubCaption: page:%d, text=%s\n",w,line.gettoken_str(2));
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_WRITEUNINSTALLER:
      if (uninstall_mode)
      {
        ERROR_MSG("WriteUninstaller only valid from install, not from uninstall.\n");
        PRINTHELP()
      }
      uninstaller_writes_used++;
      ent.which=EW_WRITEUNINSTALLER;
      ent.offsets[0]=add_string_main(line.gettoken_str(1));
      ent.offsets[1]=0; // uninstall section 0
      ent.offsets[2]=0;
      if (ent.offsets[0]<0) PRINTHELP()
      SCRIPT_MSG("WriteUninstaller: \"%s\"\n",line.gettoken_str(1));
    return add_entry(&ent);
#else//!NSIS_CONFIG_UNINSTALL_SUPPORT
    case TOK_WRITEUNINSTALLER:
    case TOK_UNINSTCAPTION:
    case TOK_UNINSTICON:
    case TOK_UNINSTTEXT:
    case TOK_UNINSTSUBCAPTION:
      ERROR_MSG("Error: %s specified, NSIS_CONFIG_UNINSTALL_SUPPORT not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif
    
    
    
    // section/function shit
    ///////////////////////////////////////////////////////////////////////////////

    case TOK_SECTION:
	{
	  int a=1,ex = 0;
	  if (!strcmp(line.gettoken_str(1),"/e"))
      {
		ex = 1;
		a++;
	  }
      SCRIPT_MSG("Section: \"%s\"",line.gettoken_str(a));
      if (line.gettoken_str(a+1)[0]) SCRIPT_MSG(" ->(%s)",line.gettoken_str(a+1));
      SCRIPT_MSG("\n");
#ifndef NSIS_CONFIG_UNINSTALL_SUPPORT
      if (!stricmp(line.gettoken_str(a),"uninstall"))
      {
        ERROR_MSG("Error: Uninstall section declared, no NSIS_CONFIG_UNINSTALL_SUPPORT\n");
        return PS_ERROR;
      }
#endif

    if (line.gettoken_str(a)[0]=='-') return add_section("",curfilename,linecnt,line.gettoken_str(a+1),ex);
    return add_section(line.gettoken_str(a),curfilename,linecnt,line.gettoken_str(2),ex);
	}
    case TOK_SECTIONEND:
      SCRIPT_MSG("SectionEnd\n");
    return section_end();
    case TOK_SECTIONIN:
      {
        SCRIPT_MSG("SectionIn: ");
        int wt;
        for (wt = 1; wt < line.getnumtokens(); wt ++)
        {
          char *p=line.gettoken_str(wt);
          if (p[0]=='R' && p[1]=='O')
          {
            if (section_add_flags(DFS_SET|DFS_RO) != PS_OK) return PS_ERROR;
            SCRIPT_MSG("[RO] ");
          }
          else
          {
            int x=atoi(p)-1;
            if (x >= 0 && x < NSIS_MAX_INST_TYPES)
            {
              if (section_add_flags(1<<x) != PS_OK) return PS_ERROR;
              SCRIPT_MSG("[%d] ",x);
            }
            else if (x < 0)
            {
              PRINTHELP()
            }
            else
            {
              ERROR_MSG("Error: SectionIn section %d out of range 1-%d\n",x+1,NSIS_MAX_INST_TYPES); 
              return PS_ERROR;
            }
            p++;
          }
        }
        SCRIPT_MSG("\n");
      }
    return PS_OK;
    case TOK_SUBSECTIONEND:
    case TOK_SUBSECTION:
    {
      char buf[1024];
	  int a=1,ex = 0;
	  if (!strcmp(line.gettoken_str(1),"/e"))
      {
		ex = 1;
		a++;
	  }
      wsprintf(buf,"-%s",line.gettoken_str(a));
      if (which_token == TOK_SUBSECTION && !line.gettoken_str(a)[0]) PRINTHELP()

      if (which_token == TOK_SUBSECTIONEND)
      {
        subsection_open_cnt--;
        if (subsection_open_cnt<0)
        {
          ERROR_MSG("SubSectionEnd: no SubSections are open\n");
          return PS_ERROR;
        }
      }
      else
        subsection_open_cnt++;

      SCRIPT_MSG("%s %s\n",line.gettoken_str(0),line.gettoken_str(a));
      return add_section(buf,curfilename,linecnt,"",ex);
    }
    case TOK_FUNCTION:
      if (!line.gettoken_str(1)[0]) PRINTHELP()
      if (line.gettoken_str(1)[0]==':' || line.gettoken_str(1)[0]=='/')
      {
        ERROR_MSG("Function: function name cannot begin with : or /.\n");
        PRINTHELP()
      }
      SCRIPT_MSG("Function: \"%s\"\n",line.gettoken_str(1));
#ifndef NSIS_CONFIG_UNINSTALL_SUPPORT
      if (!strnicmp(line.gettoken_str(1),"un.",3))
      {
        ERROR_MSG("Error: Uninstall function declared, no NSIS_CONFIG_UNINSTALL_SUPPORT\n");
        return PS_ERROR;
      }
#endif
      return add_function(line.gettoken_str(1));
    case TOK_FUNCTIONEND:
      SCRIPT_MSG("FunctionEnd\n");
    return function_end();

    // flag setters
    ///////////////////////////////////////////////////////////////////////////////

    case TOK_SETDATESAVE:
      build_datesave=line.gettoken_enum(1,"off\0on\0");
      if (build_datesave==-1) PRINTHELP()
      SCRIPT_MSG("SetDateSave: %s\n",line.gettoken_str(1));    
    return PS_OK;
    case TOK_SETOVERWRITE:
      build_overwrite=line.gettoken_enum(1,"on\0off\0try\0ifnewer\0");
      if (build_overwrite==-1) PRINTHELP()
      SCRIPT_MSG("SetOverwrite: %s\n",line.gettoken_str(1));
    return PS_OK;
    case TOK_SETCOMPRESS:
      build_compress=line.gettoken_enum(1,"off\0auto\0force\0");
      if (build_compress==-1) PRINTHELP()
      SCRIPT_MSG("SetCompress: %s\n",line.gettoken_str(1));
    return PS_OK;
    case TOK_DBOPTIMIZE:
      build_optimize_datablock=line.gettoken_enum(1,"off\0on\0");
      if (build_optimize_datablock==-1) PRINTHELP()
      SCRIPT_MSG("SetDatablockOptimize: %s\n",line.gettoken_str(1));
    return PS_OK;
    case TOK_ADDSIZE:
      {
        int s;
        int size_kb=line.gettoken_int(1,&s);
        if (!s) PRINTHELP()
        SCRIPT_MSG("AddSize: %d kb\n");
        section_add_size_kb(size_kb);
      }
    return PS_OK;
    case TOK_SUBCAPTION:
      {
        int s;
        int w=line.gettoken_int(1,&s);
        if (!s || w < 0 || w > 4) PRINTHELP()
        build_header.common.subcaption_ptrs[w]=add_string_main(line.gettoken_str(2));
        SCRIPT_MSG("SubCaption: page:%d, text=%s\n",w,line.gettoken_str(2));
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_FILEERRORTEXT:
#ifdef NSIS_SUPPORT_FILE
      build_header.common.fileerrtext_ptr=add_string_main(line.gettoken_str(1));
      SCRIPT_MSG("FileErrorText: \"%s\"\n",line.gettoken_str(1));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
#else
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_FILE not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif
    case TOK_BRANDINGTEXT:
      build_header.common.branding_ptr=add_string_main(line.gettoken_str(1),0);
      SCRIPT_MSG("BrandingText: \"%s\"\n",line.gettoken_str(1));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_MISCBUTTONTEXT:
      build_header.backbutton_ptr=add_string_main(line.gettoken_str(1),0);
      build_header.nextbutton_ptr=add_string_main(line.gettoken_str(2),0);
      build_header.common.cancelbutton_ptr=add_string_main(line.gettoken_str(3),0);
      build_header.common.closebutton_ptr=add_string_main(line.gettoken_str(4),0);
      SCRIPT_MSG("MiscButtonText: back=\"%s\" next=\"%s\" cancel=\"%s\" close=\"%s\"\n",line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3),line.gettoken_str(4));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_SPACETEXTS:
      if (!lstrcmp(line.gettoken_str(1), "none")) {
        build_header.spacerequired_ptr=-2; // No space text
        SCRIPT_MSG("SpaceTexts: none\n");
      }
      else {
        build_header.spacerequired_ptr=add_string_main(line.gettoken_str(1),0);
        build_header.spaceavailable_ptr=add_string_main(line.gettoken_str(2),0);
        SCRIPT_MSG("SpaceTexts: required=\"%s\" available=\"%s\"\n",line.gettoken_str(1),line.gettoken_str(2));
      }
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_INSTBUTTONTEXT:
      build_header.installbutton_ptr=add_string_main(line.gettoken_str(1),0);
      SCRIPT_MSG("InstallButtonText: \"%s\"\n",line.gettoken_str(1));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_DETAILSBUTTONTEXT:
      build_header.common.showdetailsbutton_ptr=add_string_main(line.gettoken_str(1),0);
      SCRIPT_MSG("DetailsButtonText: \"%s\"\n",line.gettoken_str(1));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_COMPLETEDTEXT:
      build_header.common.completed_ptr=add_string_main(line.gettoken_str(1),0);
      SCRIPT_MSG("CompletedText: \"%s\"\n",line.gettoken_str(1));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
    case TOK_UNINSTBUTTONTEXT:
#ifdef NSIS_CONFIG_UNINSTALL_SUPPORT
      build_uninst.uninstbutton_ptr=add_string_uninst(line.gettoken_str(1),0);
      SCRIPT_MSG("UninstButtonText: \"%s\"\n",line.gettoken_str(1));
    return make_sure_not_in_secorfunc(line.gettoken_str(0));
#else
      ERROR_MSG("Error: %s specified, NSIS_CONFIG_UNINSTALL_SUPPORT not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif

    // instructions
    ///////////////////////////////////////////////////////////////////////////////
    case TOK_NOP: 
      SCRIPT_MSG("Nop\n");
      ent.which=EW_NOP;
    return add_entry(&ent);
    case TOK_GOTO: 
      ent.which=EW_NOP;
      if (process_jump(line,1,&ent.offsets[0])) PRINTHELP()
      SCRIPT_MSG("Goto: %s\n",line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_SETSHELLVARCONTEXT:
      ent.which=EW_SETSFCONTEXT;
      ent.offsets[0]=line.gettoken_enum(1,"current\0all\0");
      if (ent.offsets[0]<0) PRINTHELP()
      SCRIPT_MSG("SetShellVarContext: %s\n",line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_RET:
      SCRIPT_MSG("Return\n");
      ent.which=EW_RET;
    return add_entry(&ent);
    case TOK_CALL:
      if (!line.gettoken_str(1)[0] || (line.gettoken_str(1)[0]==':' && !line.gettoken_str(1)[1] )) PRINTHELP()
#ifdef NSIS_CONFIG_UNINSTALL_SUPPORT
      if (uninstall_mode && strnicmp(line.gettoken_str(1),"un.",3) && (line.gettoken_enum(1,usrvars) < 0))
      {
        ERROR_MSG("Call must be used with function names starting with \"un.\" in the uninstall section.\n");
        PRINTHELP()
      }
      if (!uninstall_mode && !strnicmp(line.gettoken_str(1),"un.",3))
      {
        ERROR_MSG("Call must not be used with functions starting with \"un.\" in the non-uninstall sections.\n");
        PRINTHELP()
      }
#endif
      ent.which=EW_CALL;
      ent.offsets[1]=0;
      {
        int v;
        if ((v=line.gettoken_enum(1,usrvars))>=0)
        {
          ent.offsets[0]=-v-2;
        }
        else
        {
          if (line.gettoken_str(1)[0] == ':') 
          {
            ent.offsets[1]=1;           
            ent.offsets[0]=ns_label.add(line.gettoken_str(1)+1,0);
          }
          else ent.offsets[0]=ns_func.add(line.gettoken_str(1),0);
        }
      }
      SCRIPT_MSG("Call \"%s\"\n",line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_SETOUTPATH:
      {
        char *p=line.gettoken_str(1);
        if (*p == '-') cur_out_path[0]=0;
        else
        { 
          if (p[0] == '\\' && p[1] != '\\') p++;
          strncpy(cur_out_path,p,1024-1);
          cur_out_path[1024-1]=0;
          if (*CharPrev(cur_out_path,cur_out_path+strlen(cur_out_path))=='\\') 
            *CharPrev(cur_out_path,cur_out_path+strlen(cur_out_path))=0; // remove trailing slash
        }
        if (!cur_out_path[0]) strcpy(cur_out_path,"$INSTDIR");
        SCRIPT_MSG("SetOutPath: \"%s\"\n",cur_out_path);
        ent.which=EW_CREATEDIR;
        ent.offsets[0]=add_string(cur_out_path);
        ent.offsets[1]=1;
      }
    return add_entry(&ent);
    case TOK_CREATEDIR:
      {
        char out_path[1024];
        char *p=line.gettoken_str(1);
        if (*p == '-') out_path[0]=0;
        else
        { 
          if (p[0] == '\\' && p[1] != '\\') p++;
          strncpy(out_path,p,1024-1);
          if (*CharPrev(out_path,out_path+strlen(out_path))=='\\') 
            *CharPrev(out_path,out_path+strlen(out_path))=0; // remove trailing slash
        }
        if (!*out_path) PRINTHELP()
        SCRIPT_MSG("CreateDirectory: \"%s\"\n",out_path);
        ent.which=EW_CREATEDIR;
        ent.offsets[0]=add_string(out_path);
      }
    return add_entry(&ent);
    case TOK_EXEC:
    case TOK_EXECWAIT:
#ifdef NSIS_SUPPORT_EXECUTE
      ent.which=EW_EXECUTE;
      ent.offsets[0]=add_string(line.gettoken_str(1));
      ent.offsets[1] = 0;
      if (which_token == TOK_EXECWAIT) 
      {
        ent.offsets[1]=1;
        ent.offsets[2]=line.gettoken_enum(2,usrvars);
        if (line.gettoken_str(2)[0] && ent.offsets[2]<0) PRINTHELP()
      }
      SCRIPT_MSG("%s: \"%s\" (->%s)\n",ent.offsets[1]?"ExecWait":"Exec",line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);
#else//!NSIS_SUPPORT_EXECUTE
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_EXECUTE not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_SUPPORT_EXECUTE
    case TOK_EXECSHELL: // this uses improvements of Andras Varga
#ifdef NSIS_SUPPORT_SHELLEXECUTE
      ent.which=EW_SHELLEXEC;
      ent.offsets[0]=add_string(line.gettoken_str(1));
      ent.offsets[1]=add_string(line.gettoken_str(2));
      ent.offsets[2]=add_string(line.gettoken_str(3));
      ent.offsets[3]=SW_SHOWNORMAL;
      if (line.getnumtokens() > 4)
      {
        int tab[3]={SW_SHOWNORMAL,SW_SHOWMAXIMIZED,SW_SHOWMINIMIZED};
        int a=line.gettoken_enum(4,"SW_SHOWNORMAL\0SW_SHOWMAXIMIZED\0SW_SHOWMINIMIZED\0");
        if (a < 0) PRINTHELP()
        ent.offsets[3]=tab[a];
      }
      SCRIPT_MSG("ExecShell: %s: \"%s\" \"%s\" %s\n",line.gettoken_str(1),line.gettoken_str(2),
                                                 line.gettoken_str(3),line.gettoken_str(4));
    return add_entry(&ent);
#else//!NSIS_SUPPORT_SHELLEXECUTE
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_SHELLEXECUTE not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_SUPPORT_SHELLEXECUTE
    case TOK_CALLINSTDLL:
    case TOK_REGDLL:
    case TOK_UNREGDLL:
#ifndef NSIS_SUPPORT_ACTIVEXREG
      ERROR_MSG("%s: support not compiled in (NSIS_SUPPORT_ACTIVEXREG)\n",line.gettoken_str(0));
      return PS_ERROR;
#else//NSIS_SUPPORT_ACTIVEXREG
      ent.which=EW_REGISTERDLL;
      ent.offsets[0]=add_string(line.gettoken_str(1));
      if (which_token == TOK_UNREGDLL) 
      {
        ent.offsets[1]=add_string("DllUnregisterServer"); 
        ent.offsets[2]=add_string("Unregistering: ");
      }
      else if (which_token == TOK_CALLINSTDLL)
      {
        ent.offsets[1] = add_string(line.gettoken_str(2));
        if (ent.offsets[1]<0) PRINTHELP()
        ent.offsets[2]=-1;
      }
      else // register
      {
        ent.offsets[1] = add_string(line.gettoken_str(2));
        if (ent.offsets[1]<0) ent.offsets[1]=add_string("DllRegisterServer");
        ent.offsets[2]=add_string("Registering: ");
      }

      SCRIPT_MSG("%s: \"%s\" %s\n",line.gettoken_str(0),line.gettoken_str(1), line.gettoken_str(2));
    return add_entry(&ent);
#endif//NSIS_SUPPORT_ACTIVEXREG
    case TOK_RENAME:
#ifdef NSIS_SUPPORT_RENAME
      {
        int a=1;
        ent.which=EW_RENAME;
        if (!stricmp(line.gettoken_str(1),"/REBOOTOK"))
        {
          ent.offsets[2]=1;
          a++;
#ifndef NSIS_SUPPORT_MOVEONREBOOT
          ERROR_MSG("Error: /REBOOTOK specified, NSIS_SUPPORT_MOVEONREBOOT not defined\n");
          PRINTHELP()
#endif
        }
        else if (line.gettoken_str(1)[0]=='/')
        {
          a=line.getnumtokens(); // cause usage to go here:
        }
        if (line.getnumtokens()!=a+2) PRINTHELP()
        ent.offsets[0]=add_string(line.gettoken_str(a));
        ent.offsets[1]=add_string(line.gettoken_str(a+1));
        SCRIPT_MSG("Rename: %s%s->%s\n",ent.offsets[2]?"/REBOOTOK ":"",line.gettoken_str(a),line.gettoken_str(a+1));
      }
    return add_entry(&ent);
#else//!NSIS_SUPPORT_RENAME
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_RENAME not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_SUPPORT_RENAME
    case TOK_MESSAGEBOX:
#ifdef NSIS_SUPPORT_MESSAGEBOX
      {
        #define MBD(x) {x,#x},
        struct 
        {
          int id;
          char *str;
        } list[]=
        {
          MBD(MB_ABORTRETRYIGNORE)
          MBD(MB_OK)
          MBD(MB_OKCANCEL)
          MBD(MB_RETRYCANCEL)
          MBD(MB_YESNO)
          MBD(MB_YESNOCANCEL)
          MBD(MB_ICONEXCLAMATION)
          MBD(MB_ICONINFORMATION)
          MBD(MB_ICONQUESTION)
          MBD(MB_ICONSTOP)
          MBD(MB_TOPMOST)
          MBD(MB_SETFOREGROUND)
          MBD(MB_RIGHT)
          MBD(MB_DEFBUTTON1)
          MBD(MB_DEFBUTTON2)
          MBD(MB_DEFBUTTON3)
          MBD(MB_DEFBUTTON4)
        };
        #undef MBD
        int r=0;
        int x;
        char *p=line.gettoken_str(1);

        while (*p)
        {
          char *np=p;
          while (*np && *np != '|') np++;
          if (*np) *np++=0;
          for (x  =0 ; x < sizeof(list)/sizeof(list[0]) && strcmp(list[x].str,p); x ++);
          if (x < sizeof(list)/sizeof(list[0]))
          {
            r|=list[x].id;
          }
          else PRINTHELP()
          p=np;
        }
        ent.which=EW_MESSAGEBOX;
        ent.offsets[0]=r;
        ent.offsets[1]=add_string(line.gettoken_str(2));
        int rettab[] = 
        {
          0,IDABORT,IDCANCEL,IDIGNORE,IDNO,IDOK,IDRETRY,IDYES
        };
        const char *retstr="0\0IDABORT\0IDCANCEL\0IDIGNORE\0IDNO\0IDOK\0IDRETRY\0IDYES\0";
        if (line.getnumtokens() > 3)
        {
          ent.offsets[2]=line.gettoken_enum(3,retstr);
          if (ent.offsets[2] < 0) PRINTHELP()
          ent.offsets[2] = rettab[ent.offsets[2]];
          if (process_jump(line,4,&ent.offsets[3])) PRINTHELP()
          if (line.getnumtokens() > 5)
          {
            int v=line.gettoken_enum(5,retstr);
            if (v < 0) PRINTHELP()
            ent.offsets[2] |= rettab[v]<<16;
            if (process_jump(line,6,&ent.offsets[4])) PRINTHELP()
          }
        }
        SCRIPT_MSG("MessageBox: %d: \"%s\"",r,line.gettoken_str(2));
        if (line.getnumtokens()>4) SCRIPT_MSG(" (on %s goto %s)",line.gettoken_str(3),line.gettoken_str(4));
        SCRIPT_MSG("\n");
      }      
    return add_entry(&ent);
#else//!NSIS_SUPPORT_MESSAGEBOX
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_MESSAGEBOX not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_SUPPORT_MESSAGEBOX
    case TOK_CREATESHORTCUT:
#ifdef NSIS_SUPPORT_CREATESHORTCUT
      ent.which=EW_CREATESHORTCUT;
      ent.offsets[0]=add_string(line.gettoken_str(1));
      ent.offsets[1]=add_string(line.gettoken_str(2));
      ent.offsets[2]=add_string(line.gettoken_str(3));
      ent.offsets[3]=add_string(line.gettoken_str(4));
      int s;
      ent.offsets[4]=line.gettoken_int(5,&s)&0xff;
      if (!s)
      {
        if (line.getnumtokens() > 5)
        {
          ERROR_MSG("CreateShortCut: cannot interpret icon index\n");
          PRINTHELP()
        }
        ent.offsets[4]=0;
      }
      if (line.getnumtokens() > 6)
      {
        int tab[3]={SW_SHOWNORMAL,SW_SHOWMAXIMIZED,SW_SHOWMINNOACTIVE/*SW_SHOWMINIMIZED doesn't work*/};
        int a=line.gettoken_enum(6,"SW_SHOWNORMAL\0SW_SHOWMAXIMIZED\0SW_SHOWMINIMIZED\0");
        if (a < 0)
        {
          ERROR_MSG("CreateShortCut: unknown show mode \"%s\"\n",line.gettoken_str(6));
          PRINTHELP()
        }
        ent.offsets[4]|=tab[a]<<8;
      }
      if (line.getnumtokens() > 7)
      {
        char *s=line.gettoken_str(7);
        if (*s)
        {
          int c=0;
          if (strstr(s,"ALT|")) ent.offsets[4]|=HOTKEYF_ALT << 24;
          if (strstr(s,"CONTROL|")) ent.offsets[4]|=HOTKEYF_CONTROL << 24;
          if (strstr(s,"EXT|")) ent.offsets[4]|=HOTKEYF_EXT << 24;
          if (strstr(s,"SHIFT|")) ent.offsets[4]|=HOTKEYF_SHIFT << 24;
          while (strstr(s,"|"))
          {
            s=strstr(s,"|")+1;
          }
          if ((s[0] == 'f' || s[0] == 'F') && (s[1] >= '1' && s[1] <= '9'))
          {
            c=VK_F1-1+atoi(s+1);
            if (atoi(s+1) < 1 || atoi(s+1) > 24)
            {
              warning("CreateShortCut: F-key \"%s\" out of range (%s:%d)\n",s,curfilename,linecnt);
            }
          }
          else if (s[0] >= 'a' && s[0] <= 'z' && !s[1])
            c=s[0]+'A'-'a';
          else if (((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= '0' && s[0] <= '9')) && !s[1])
            c=s[0];
          else
          {
            c=s[0];
            warning("CreateShortCut: unrecognized hotkey \"%s\" (%s:%d)\n",s,curfilename,linecnt);
          }
          ent.offsets[4] |= (c) << 16;
        }
      }
      SCRIPT_MSG("CreateShortCut: \"%s\"->\"%s\" %s  icon:%s,%d, showmode=0x%X, hotkey=0x%X\n",
        line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3),
        line.gettoken_str(4),ent.offsets[4]&0xff,(ent.offsets[4]>>8)&0xff,ent.offsets[4]>>16);
    return add_entry(&ent);
#else//!NSIS_SUPPORT_CREATESHORTCUT
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_CREATESHORTCUT not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//NSIS_SUPPORT_CREATESHORTCUT
#ifdef NSIS_SUPPORT_HWNDS
    case TOK_FINDWINDOW:
      ent.which=EW_FINDWINDOW;
      ent.offsets[0]=line.gettoken_enum(1,usrvars);
      if (ent.offsets[0] < 0) PRINTHELP()
      ent.offsets[1]=add_string(line.gettoken_str(2));
      ent.offsets[2]=add_string(line.gettoken_str(3));
      ent.offsets[3]=add_string(line.gettoken_str(4));
      ent.offsets[4]=add_string(line.gettoken_str(5));
      SCRIPT_MSG("FindWindow: output=%s, class=\"%s\", text=\"%s\" hwndparent=\"%s\" hwndafter=\"%s\"\n",
        line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3),line.gettoken_str(4),line.gettoken_str(5));
    return add_entry(&ent);
    case TOK_SENDMESSAGE:
      ent.which=EW_SENDMESSAGE;
      SCRIPT_MSG("SendMessage:");
      ent.offsets[0]=line.gettoken_enum(5,usrvars);
      if (ent.offsets[0]>=0)
      {
        SCRIPT_MSG("(->%s)",line.gettoken_str(5));
      }

      ent.offsets[1]=add_string(line.gettoken_str(1));
      ent.offsets[2]=add_string(line.gettoken_str(2));
      ent.offsets[3]=add_string(line.gettoken_str(3));
      ent.offsets[4]=add_string(line.gettoken_str(4));

      SCRIPT_MSG("(%s,%s,%s,%s)\n",line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3),line.gettoken_str(4));
    return add_entry(&ent);
    case TOK_ISWINDOW:
      ent.which=EW_ISWINDOW;
      ent.offsets[0]=add_string(line.gettoken_str(1));
      if (process_jump(line,2,&ent.offsets[1])||
          process_jump(line,3,&ent.offsets[2])) PRINTHELP()
      SCRIPT_MSG("IsWindow(%s): %s:%s\n",line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3));
    return add_entry(&ent);
    case TOK_SETDLGITEMTEXT:
      ent.which=EW_SETDLGITEMTEXT;
      ent.offsets[0]=line.gettoken_int(1);
      ent.offsets[1]=add_string(line.gettoken_str(2));
      SCRIPT_MSG("SetDlgItemText: item=%s text=%s\n",line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);
#else//!NSIS_SUPPORT_HWNDS
    case TOK_ISWINDOW:
    case TOK_SENDMESSAGE:
    case TOK_FINDWINDOW:
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_HWNDS not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_SUPPORT_HWNDS
    case TOK_DELETE:
#ifdef NSIS_SUPPORT_DELETE
      {
        int a=1;
        ent.which=EW_DELETEFILE;
        if (!stricmp(line.gettoken_str(a),"/REBOOTOK"))
        {
          a++;
          ent.offsets[1]=1;
#ifndef NSIS_SUPPORT_MOVEONREBOOT
          ERROR_MSG("Error: /REBOOTOK specified, NSIS_SUPPORT_MOVEONREBOOT not defined\n");
          PRINTHELP()
#endif
        }
        else if (line.gettoken_str(1)[0]=='/')
        {
          a=line.getnumtokens();
        }
        if (line.getnumtokens() != a+1) PRINTHELP()
        ent.offsets[0]=add_string(line.gettoken_str(a));
        SCRIPT_MSG("Delete: %s\"%s\"\n",ent.offsets[1]?"/REBOOTOK ":"",line.gettoken_str(a));
      }
    return add_entry(&ent);
#else//!NSIS_SUPPORT_DELETE
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_DELETE not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_SUPPORT_DELETE
    case TOK_RMDIR:
#ifdef NSIS_SUPPORT_RMDIR
      {
        int a=1;
        ent.which=EW_RMDIR;
        if (!stricmp(line.gettoken_str(1),"/r"))
        {
          if (line.getnumtokens() < 3) PRINTHELP()
          a++;
          ent.offsets[1]=1;
        }
        else if (line.gettoken_str(1)[0]=='/') PRINTHELP()
        ent.offsets[0]=add_string(line.gettoken_str(a));
        SCRIPT_MSG("RMDir: %s\"%s\"\n",ent.offsets[1]?"/r " : "",line.gettoken_str(a));
      }
    return add_entry(&ent);
#else//!NSIS_SUPPORT_RMDIR
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_RMDIR not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_SUPPORT_RMDIR
    case TOK_FILE:
#ifdef NSIS_SUPPORT_FILE
      {
        int a=1,attrib=0,rec=0;
        if (!stricmp(line.gettoken_str(a),"/a"))
        {
          attrib=1;
          a++;
        }
        if (!stricmp(line.gettoken_str(a),"/r"))
        {
          rec=1;
          a++;
        }
        else if (!strnicmp(line.gettoken_str(a),"/oname=",7))
        {
          char *on=line.gettoken_str(a)+7;
          a++;
          if (!*on||line.getnumtokens()!=a+1||strstr(on,"*") || strstr(on,"?")) PRINTHELP()

          int tf=0;
          int v=do_add_file(line.gettoken_str(a), attrib, 0, linecnt,&tf,on);
          if (v != PS_OK) return v;
          if (tf > 1) PRINTHELP()
          if (!tf)
          {
            ERROR_MSG("File: \"%s\" -> no files found.\n",line.gettoken_str(a));
            PRINTHELP()
          }

          return PS_OK;
        }
        else if (line.gettoken_str(a)[0] == '/') PRINTHELP()
        if (line.getnumtokens()<a+1) PRINTHELP()
        while (a < line.getnumtokens())
        {
          if (line.gettoken_str(a)[0]=='/') PRINTHELP()
          char buf[32];
          char *t=line.gettoken_str(a++);
          if (t[0] && CharNext(t)[0] == ':' && CharNext(t)[1] == '\\' && !CharNext(t)[2])
          {
            strcpy(buf,"X:\\*.*");
            buf[0]=t[0];
            t=buf;
          }
          int tf=0;
          int v=do_add_file(t, attrib, rec, linecnt,&tf);
          if (v != PS_OK) return v;
          if (!tf)
          {
            ERROR_MSG("File: \"%s\" -> no files found.\n",t);
            PRINTHELP()
          }
        }
      }
    return PS_OK;
#else//!NSIS_SUPPORT_FILE
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_FILE not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_SUPPORT_FILE
#ifdef NSIS_SUPPORT_COPYFILES
    case TOK_COPYFILES:
      {
        ent.which=EW_COPYFILES;
        ent.offsets[2]=FOF_NOCONFIRMATION|FOF_NOCONFIRMMKDIR|FOF_NOERRORUI|FOF_SIMPLEPROGRESS;
        
        int a=1;
        int x;
        for (x = 0; x < 2; x ++)
        {
          if (!stricmp(line.gettoken_str(a),"/SILENT"))
          {
            a++;
            ent.offsets[2]&=~FOF_SIMPLEPROGRESS;
            ent.offsets[2]|=FOF_SILENT;
          }
          else if (!stricmp(line.gettoken_str(a),"/FILESONLY"))
          {
            a++;
            ent.offsets[2]|=FOF_FILESONLY;
          }
          else if (line.gettoken_str(a)[0]=='/') PRINTHELP()
          else break;
        } 
        ent.offsets[0]=add_string(line.gettoken_str(a));
        ent.offsets[1]=add_string(line.gettoken_str(a+1));
        int s;
        int size_kb=line.gettoken_int(a+2,&s);
        if (!s && line.gettoken_str(a+2)[0]) PRINTHELP()
        section_add_size_kb(size_kb);
        SCRIPT_MSG("CopyFiles: %s\"%s\" -> \"%s\", size=%iKB\n",ent.offsets[2]?"(silent) ":"", line.gettoken_str(a),line.gettoken_str(a+1),size_kb);
      }
    return add_entry(&ent);
#else//!NSIS_SUPPORT_COPYFILES
    case TOK_COPYFILES:
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_COPYFILES not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_SUPPORT_COPYFILES

    case TOK_SETFILEATTRIBUTES:
      {
        #define MBD(x) {x,#x},
        struct 
        {
          int id;
          char *str;
        } list[]=
        {
          MBD(FILE_ATTRIBUTE_NORMAL)
          MBD(FILE_ATTRIBUTE_ARCHIVE)
          MBD(FILE_ATTRIBUTE_HIDDEN)
          MBD(FILE_ATTRIBUTE_OFFLINE)
          MBD(FILE_ATTRIBUTE_READONLY)
          MBD(FILE_ATTRIBUTE_SYSTEM)
          MBD(FILE_ATTRIBUTE_TEMPORARY)
          {FILE_ATTRIBUTE_NORMAL,"NORMAL"},
          {FILE_ATTRIBUTE_ARCHIVE,"ARCHIVE"},
          {FILE_ATTRIBUTE_HIDDEN,"HIDDEN"},
          {FILE_ATTRIBUTE_OFFLINE,"OFFLINE"},
          {FILE_ATTRIBUTE_READONLY,"READONLY"},
          {FILE_ATTRIBUTE_SYSTEM,"SYSTEM"},
          {FILE_ATTRIBUTE_TEMPORARY,"TEMPORARY"},
          {FILE_ATTRIBUTE_NORMAL,"0"},
        };
        #undef MBD
        int r=0;
        int x;
        char *p=line.gettoken_str(2);

        while (*p)
        {
          char *np=p;
          while (*np && *np != '|') np++;
          if (*np) *np++=0;
          for (x  =0 ; x < sizeof(list)/sizeof(list[0]) && stricmp(list[x].str,p); x ++);

          if (x < sizeof(list)/sizeof(list[0]))
          {
            r|=list[x].id;
          }
          else PRINTHELP()
          p=np;
        }
        ent.which=EW_SETFILEATTRIBUTES;
        ent.offsets[0]=add_string(line.gettoken_str(1));
        ent.offsets[1]=r;
      }
    return add_entry(&ent);
    case TOK_SLEEP:
      {
        ent.which=EW_SLEEP;        
        ent.offsets[0]=add_string(line.gettoken_str(1));
        SCRIPT_MSG("Sleep: %s ms\n",line.gettoken_str(1));
      }
    return add_entry(&ent);
    case TOK_BRINGTOFRONT:
      ent.which=EW_BRINGTOFRONT;        
      SCRIPT_MSG("BringToFront\n");
    return add_entry(&ent);
    case TOK_HIDEWINDOW:
      ent.which=EW_HIDEWINDOW;        
      SCRIPT_MSG("HideWindow\n");
    return add_entry(&ent);
    case TOK_IFFILEEXISTS:
      ent.which=EW_IFFILEEXISTS;
      ent.offsets[0] = add_string(line.gettoken_str(1));
      if (process_jump(line,2,&ent.offsets[1]) || 
          process_jump(line,3,&ent.offsets[2])) PRINTHELP()
      SCRIPT_MSG("IfFileExists: \"%s\" ? %s : %s\n",line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3));
    return add_entry(&ent);
    case TOK_QUIT:
      ent.which=EW_QUIT;
      SCRIPT_MSG("Quit\n");
    return add_entry(&ent);
    case TOK_ABORT:
      ent.which=EW_ABORT;
      ent.offsets[0] = add_string(line.gettoken_str(1));
      SCRIPT_MSG("Abort: \"%s\"\n",line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_SETDETAILSVIEW:
      {
        int v=line.gettoken_enum(1,"hide\0show\0");
        ent.which=EW_CHDETAILSVIEW;
        if (v < 0) PRINTHELP()
        ent.offsets[0] = v?SW_SHOWNA:SW_HIDE;
        ent.offsets[1] = v?SW_HIDE:SW_SHOWNA;
        SCRIPT_MSG("SetDetailsView: %s\n",line.gettoken_str(1));
      }
    return add_entry(&ent);
    case TOK_SETDETAILSPRINT:
      ent.which=EW_UPDATETEXT;
      ent.offsets[0] = -1;
      ent.offsets[1] = line.gettoken_enum(1,"none\0listonly\0textonly\0both\0");
      if (ent.offsets[1] < 0) PRINTHELP()
      if (!ent.offsets[1]) ent.offsets[1]=4;
      SCRIPT_MSG("SetDetailsPrint: %s\n",line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_SETAUTOCLOSE:
      ent.which=EW_SETWINDOWCLOSE;
      ent.offsets[0] = line.gettoken_enum(1,"false\0true\0");
      if (ent.offsets[0] < 0) PRINTHELP()
      SCRIPT_MSG("SetAutoClose: %s\n",line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_IFERRORS:
      ent.which=EW_IFERRORS;
      if (process_jump(line,1,&ent.offsets[0]) || 
          process_jump(line,2,&ent.offsets[1])) PRINTHELP()
      SCRIPT_MSG("IfErrors ?%s:%s\n",line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);
    case TOK_CLEARERRORS:
      ent.which=EW_IFERRORS;
      SCRIPT_MSG("ClearErrors\n");
    return add_entry(&ent);
    case TOK_SETERRORS:
      ent.which=EW_IFERRORS;
      ent.offsets[2]=1;
      SCRIPT_MSG("SetErrors\n");
    return add_entry(&ent);
#ifdef NSIS_SUPPORT_STROPTS
    case TOK_STRLEN:
      ent.which=EW_STRLEN;
      ent.offsets[0]=line.gettoken_enum(1,usrvars);
      ent.offsets[1]=add_string(line.gettoken_str(2));
      if (ent.offsets[0] < 0) PRINTHELP()
      SCRIPT_MSG("StrLen %s \"%s\"\n",line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);
    case TOK_STRCPY:
      ent.which=EW_ASSIGNVAR;
      ent.offsets[0]=line.gettoken_enum(1,usrvars);
      ent.offsets[1]=add_string(line.gettoken_str(2));
      ent.offsets[2]=add_string(line.gettoken_str(3));
      ent.offsets[3]=add_string(line.gettoken_str(4));

      if (ent.offsets[0] < 0) PRINTHELP()
      SCRIPT_MSG("StrCpy %s \"%s\" (%s) (%s)\n",line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3),line.gettoken_str(4));
    return add_entry(&ent);
    case TOK_GETFUNCTIONADDR:
      ent.which=EW_GETFUNCTIONADDR;
      ent.offsets[0]=line.gettoken_enum(1,usrvars);
      ent.offsets[1]=ns_func.add(line.gettoken_str(2),0);
      ent.offsets[2]=-1;
      ent.offsets[3]=-1;
      if (ent.offsets[0] < 0) PRINTHELP()
      SCRIPT_MSG("GetFunctionAddress: %s %s",line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);
    case TOK_GETLABELADDR:
      ent.which=EW_GETLABELADDR;
      ent.offsets[0]=line.gettoken_enum(1,usrvars);
      if (ent.offsets[0] < 0 || process_jump(line,2,&ent.offsets[1])) PRINTHELP()
      ent.offsets[2]=-1;
      ent.offsets[3]=-1;
      SCRIPT_MSG("GetLabelAddress: %s %s",line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);
    case TOK_GETCURRENTADDR:
      ent.which=EW_ASSIGNVAR;
      ent.offsets[0]=line.gettoken_enum(1,usrvars);
      {
        char buf[32];
        wsprintf(buf,"%d",1+(uninstall_mode?build_uninst.code_size:build_header.common.num_entries));
        ent.offsets[1]=add_string(buf);
      }
      if (ent.offsets[0] < 0) PRINTHELP()
      ent.offsets[2]=-1;
      ent.offsets[3]=-1;
      SCRIPT_MSG("GetCurrentAddress: %s %s",line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_STRCMP:
      ent.which=EW_STRCMP;
      ent.offsets[0]=add_string(line.gettoken_str(1));
      ent.offsets[1]=add_string(line.gettoken_str(2));
      if (process_jump(line,3,&ent.offsets[2]) ||
          process_jump(line,4,&ent.offsets[3])) PRINTHELP()
      SCRIPT_MSG("StrCmp \"%s\" \"%s\" equal=%s, nonequal=%s\n",line.gettoken_str(1),line.gettoken_str(2), line.gettoken_str(3),line.gettoken_str(4));
    return add_entry(&ent);
    case TOK_GETDLLVERSIONLOCAL:
      {
        char buf[128];
        DWORD low, high;
        DWORD s,d;
        int flag=0;
        s=GetFileVersionInfoSize(line.gettoken_str(1),&d);
        if (s)
        {
          void *buf;
          buf=(void*)GlobalAlloc(GPTR,s);
          if (buf)
          {
            UINT uLen;
            VS_FIXEDFILEINFO *pvsf;
            if (GetFileVersionInfo(line.gettoken_str(1),0,s,buf) && VerQueryValue(buf,"\\",(void**)&pvsf,&uLen))
            {
              low=pvsf->dwFileVersionLS;
              high=pvsf->dwFileVersionMS;
              flag=1;
            }
            GlobalFree(buf);
          }
        }
        if (!flag)
        {
          ERROR_MSG("GetDLLVersionLocal: error reading version info from \"%s\"\n",line.gettoken_str(1));
          return PS_ERROR;
        }
        ent.which=EW_ASSIGNVAR;
        ent.offsets[0]=line.gettoken_enum(2,usrvars);
        wsprintf(buf,"%u",high);
        ent.offsets[1]=add_string(buf);
        ent.offsets[2]=-1;
        ent.offsets[3]=-1;
        if (ent.offsets[0]<0) PRINTHELP()
        add_entry(&ent);

        ent.offsets[0]=line.gettoken_enum(3,usrvars);
        wsprintf(buf,"%u",low);
        ent.offsets[1]=add_string(buf);
        ent.offsets[2]=-1;
        ent.offsets[3]=-1;
        if (ent.offsets[0]<0) PRINTHELP()
        SCRIPT_MSG("GetDLLVersionLocal: %s (%u,%u)->(%s,%s)\n",
          line.gettoken_str(1),high,low,line.gettoken_str(2),line.gettoken_str(3));
      }
    return add_entry(&ent);
    case TOK_GETFILETIMELOCAL:
      {
        char buf[129];
        DWORD high,low;
        int flag=0;
        HANDLE hFile=CreateFile(line.gettoken_str(1),0,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
          FILETIME ft;
          if (GetFileTime(hFile,NULL,NULL,&ft))
          {
            high=ft.dwHighDateTime;
            low=ft.dwLowDateTime;
            flag=1;
          }            
          CloseHandle(hFile);
        }
        if (!flag)
        {
          ERROR_MSG("GetFileTimeLocal: error reading date from \"%s\"\n",line.gettoken_str(1));
          return PS_ERROR;
        }

        ent.which=EW_ASSIGNVAR;
        ent.offsets[0]=line.gettoken_enum(2,usrvars);
        wsprintf(buf,"%u",high);
        ent.offsets[1]=add_string(buf);
        ent.offsets[2]=-1;
        ent.offsets[3]=-1;
        if (ent.offsets[0]<0) PRINTHELP()
        add_entry(&ent);

        ent.offsets[0]=line.gettoken_enum(3,usrvars);
        wsprintf(buf,"%u",low);
        ent.offsets[1]=add_string(buf);
        ent.offsets[2]=-1;
        ent.offsets[3]=-1;
        if (ent.offsets[0]<0) PRINTHELP()
        SCRIPT_MSG("GetFileTimeLocal: %s (%u,%u)->(%s,%s)\n",
          line.gettoken_str(1),high,low,line.gettoken_str(2),line.gettoken_str(3));
      }
    return add_entry(&ent);

#else//!NSIS_SUPPORT_STROPTS
    case TOK_GETDLLVERSIONLOCAL:
    case TOK_GETFILETIMELOCAL:
    case TOK_GETFUNCTIONADDR:
    case TOK_GETLABELADDR:
    case TOK_GETCURRENTADDR:
    case TOK_STRLEN:
    case TOK_STRCPY:
    case TOK_STRCMP:
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_STROPTS not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_SUPPORT_STROPTS
#ifdef NSIS_SUPPORT_INIFILES
    case TOK_DELETEINISEC:
    case TOK_DELETEINISTR:
      {
        char *vname="<section>";
        ent.which=EW_WRITEINI;
        ent.offsets[0]=add_string(line.gettoken_str(2)); // section name
        if (line.getnumtokens() > 3) 
        {
          vname=line.gettoken_str(3);
          ent.offsets[1]=add_string(vname); // value name
        }
        else ent.offsets[1]=-1;
        ent.offsets[2]=-1;
        ent.offsets[3]=add_string(line.gettoken_str(1));
        SCRIPT_MSG("DeleteINI%s: [%s] %s in %s\n",vname?"Str":"Sec",
          line.gettoken_str(2),vname,line.gettoken_str(1));
      }
    return add_entry(&ent);
    case TOK_WRITEINISTR:
      ent.which=EW_WRITEINI;
      ent.offsets[0]=add_string(line.gettoken_str(2));
      ent.offsets[1]=add_string(line.gettoken_str(3));
      ent.offsets[2]=add_string(line.gettoken_str(4));
      ent.offsets[3]=add_string(line.gettoken_str(1));
      SCRIPT_MSG("WriteINIStr: [%s] %s=%s in %s\n",
        line.gettoken_str(2),line.gettoken_str(3),line.gettoken_str(4),line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_READINISTR:
      ent.which=EW_READINISTR;
      ent.offsets[0]=line.gettoken_enum(1,usrvars);
      if (ent.offsets[0] < 0) PRINTHELP()
      ent.offsets[1]=add_string(line.gettoken_str(3));
      ent.offsets[2]=add_string(line.gettoken_str(4));
      ent.offsets[3]=add_string(line.gettoken_str(2));
      SCRIPT_MSG("ReadINIStr %s [%s]:%s from %s\n",line.gettoken_str(1),line.gettoken_str(3),line.gettoken_str(4),line.gettoken_str(2));
    return add_entry(&ent);
#else//!NSIS_SUPPORT_INIFILES
    case TOK_DELETEINISEC:
    case TOK_DELETEINISTR:
    case TOK_WRITEINISTR:
    case TOK_READINISTR:
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_INIFILES not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_SUPPORT_INIFILES
    case TOK_DETAILPRINT:
      ent.which=EW_UPDATETEXT;
      ent.offsets[0]=add_string(line.gettoken_str(1));
      ent.offsets[1]=0;
      SCRIPT_MSG("DetailPrint: \"%s\"\n",line.gettoken_str(1));
    return add_entry(&ent);
#ifdef NSIS_SUPPORT_FNUTIL
    case TOK_GETTEMPFILENAME:
      ent.which=EW_GETTEMPFILENAME;
      ent.offsets[0]=line.gettoken_enum(1,usrvars);
      if (ent.offsets[0]<0) PRINTHELP()
      SCRIPT_MSG("GetTempFileName -> %s\n",line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_GETFULLPATHNAME:
      {
        int a=0;
        ent.which=EW_GETFULLPATHNAME;
        if (line.getnumtokens()==4 && !stricmp(line.gettoken_str(1),"/SHORT")) a++;
        else if (line.getnumtokens()==4 || *line.gettoken_str(1)=='/') PRINTHELP()
        ent.offsets[0]=line.gettoken_enum(1+a,usrvars);
        ent.offsets[1]=add_string(line.gettoken_str(2+a));
        ent.offsets[2]=!a;
        if (ent.offsets[0]<0) PRINTHELP()
        SCRIPT_MSG("GetFullPathName: %s->%s (%d)\n",
          line.gettoken_str(2+a),line.gettoken_str(1+a),a?"sfn":"lfn");
      }
    return add_entry(&ent);
    case TOK_SEARCHPATH:
      ent.which=EW_SEARCHPATH;
      ent.offsets[0]=line.gettoken_enum(1,usrvars);
      if (ent.offsets[0] < 0) PRINTHELP()
      ent.offsets[1]=add_string(line.gettoken_str(2));
      SCRIPT_MSG("SearchPath %s %s\n",line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);
#else
    case TOK_SEARCHPATH:
    case TOK_GETTEMPFILENAME:
    case TOK_GETFULLPATHNAME:
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_FNUTIL not defined.\n",  line.gettoken_str(0));
      return PS_ERROR;
#endif
    case TOK_GETDLLVERSION: 
#ifdef NSIS_SUPPORT_GETDLLVERSION
      ent.which=EW_GETDLLVERSION;
      ent.offsets[0]=add_string(line.gettoken_str(1));
      ent.offsets[1]=line.gettoken_enum(2,usrvars);
      ent.offsets[2]=line.gettoken_enum(3,usrvars);
      if (ent.offsets[1]<0 || ent.offsets[2]<0) PRINTHELP()
      SCRIPT_MSG("GetDLLVersion: %s->%s,%s\n",
        line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3));
    return add_entry(&ent);
#else//!NSIS_SUPPORT_GETDLLVERSION
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_GETDLLVERSION not defined.\n",  line.gettoken_str(0));
      return PS_ERROR;
#endif//!NSIS_SUPPORT_GETDLLVERSION
    case TOK_GETFILETIME:
#ifdef NSIS_SUPPORT_GETFILETIME
      ent.which=EW_GETFILETIME;
      ent.offsets[0]=add_string(line.gettoken_str(1));
      ent.offsets[1]=line.gettoken_enum(2,usrvars);
      ent.offsets[2]=line.gettoken_enum(3,usrvars);
      if (ent.offsets[1]<0 || ent.offsets[2]<0) PRINTHELP()
      SCRIPT_MSG("GetFileTime: %s->%s,%s\n",
        line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3));
    return add_entry(&ent);
#else//!NSIS_SUPPORT_GETFILETIME
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_GETFILETIME not defined.\n",  line.gettoken_str(0));
      return PS_ERROR;
#endif//!NSIS_SUPPORT_GETFILETIME
#ifdef NSIS_SUPPORT_INTOPTS
    case TOK_INTOP:
      ent.which=EW_INTOP;
      ent.offsets[0]=line.gettoken_enum(1,usrvars);
      ent.offsets[3]=line.gettoken_enum(3,"+\0-\0*\0/\0|\0&\0^\0~\0!\0||\0&&\0%\0");
      if (ent.offsets[0] < 0 || ent.offsets[3]<0 || ((ent.offsets[3] == 7 || ent.offsets[3]==8) && line.getnumtokens()>4)) PRINTHELP()
      ent.offsets[1]=add_string(line.gettoken_str(2));
      if (ent.offsets[3] != 7 && ent.offsets[3] != 8) ent.offsets[2]=add_string(line.gettoken_str(4));
      SCRIPT_MSG("IntOp: %s=%s%s%s\n",line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3),line.gettoken_str(4));
    return add_entry(&ent);
    case TOK_INTFMT:
      ent.which=EW_INTFMT;
      ent.offsets[0]=line.gettoken_enum(1,usrvars);
      if (ent.offsets[0]<0) PRINTHELP()
      ent.offsets[1]=add_string(line.gettoken_str(2));
      ent.offsets[2]=add_string(line.gettoken_str(3));
      SCRIPT_MSG("IntFmt: %s->%s (fmt:%s)\n",line.gettoken_str(3),line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);
    case TOK_INTCMP:
    case TOK_INTCMPU:
      ent.which=(which_token == TOK_INTCMP) ? EW_INTCMP : EW_INTCMPU;
      ent.offsets[0]=add_string(line.gettoken_str(1));
      ent.offsets[1]=add_string(line.gettoken_str(2));
      if (process_jump(line,3,&ent.offsets[2]) ||
          process_jump(line,4,&ent.offsets[3]) || 
          process_jump(line,5,&ent.offsets[4]))  PRINTHELP()
      SCRIPT_MSG("%s %s:%s equal=%s, < %s, > %s\n",line.gettoken_str(0),
        line.gettoken_str(1),line.gettoken_str(2), line.gettoken_str(3),line.gettoken_str(4),line.gettoken_str(5));
    return add_entry(&ent);
#else//!NSIS_SUPPORT_INTOPTS
    case TOK_INTOP:
    case TOK_INTCMP:
    case TOK_INTFMT:
    case TOK_INTCMPU:
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_INTOPTS not defined.\n",  line.gettoken_str(0));
      return PS_ERROR;
#endif//!NSIS_SUPPORT_INTOPTS
#ifdef NSIS_SUPPORT_REGISTRYFUNCTIONS
    case TOK_READREGSTR:
    case TOK_READREGDWORD:
      {
        ent.which=EW_READREGSTR;
        ent.offsets[0]=line.gettoken_enum(1,usrvars);
        int k=line.gettoken_enum(2,rootkeys[0]);
        if (k == -1) k=line.gettoken_enum(2,rootkeys[1]);
        if (ent.offsets[0] < 0 || k == -1) PRINTHELP()
        ent.offsets[1]=(int)rootkey_tab[k];
        ent.offsets[2]=add_string(line.gettoken_str(3));
        ent.offsets[3]=add_string(line.gettoken_str(4));
        if (which_token == TOK_READREGDWORD) ent.offsets[4]=1;
        else ent.offsets[4]=0;
        if (line.gettoken_str(3)[0] == '\\') warning("%s: registry path name begins with \'\\\', may cause problems (%s:%d)",line.gettoken_str(0),curfilename,linecnt);

        SCRIPT_MSG("%s %s %s\\%s\\%s\n",line.gettoken_str(0), 
          line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3),line.gettoken_str(4));
      }
    return add_entry(&ent);
    case TOK_DELETEREGVALUE:
    case TOK_DELETEREGKEY:
      {
        int a=1;
        if (which_token==TOK_DELETEREGKEY)
        {
          char *s=line.gettoken_str(a);
          if (s[0] == '/')
          {
            if (stricmp(s,"/ifempty")) PRINTHELP()
            a++;
            ent.offsets[3]=1;
          }
        }
        int k=line.gettoken_enum(a,rootkeys[0]);
        if (k == -1) k=line.gettoken_enum(a,rootkeys[1]);
        if (k == -1) PRINTHELP()
        ent.which=EW_DELREG;
        ent.offsets[0]=(int)rootkey_tab[k];      
        ent.offsets[1]=add_string(line.gettoken_str(a+1));
        ent.offsets[2]=(which_token==TOK_DELETEREGKEY)?-1:add_string(line.gettoken_str(a+2));
        if (line.gettoken_str(a+1)[0] == '\\') warning("%s: registry path name begins with \'\\\', may cause problems (%s:%d)",line.gettoken_str(0),curfilename,linecnt);
        if (which_token==TOK_DELETEREGKEY)
          SCRIPT_MSG("DeleteRegKey: %s\\%s\n",line.gettoken_str(a),line.gettoken_str(a+1));
        else
          SCRIPT_MSG("DeleteRegValue: %s\\%s\\%s\n",line.gettoken_str(a),line.gettoken_str(a+1),line.gettoken_str(a+2));
      }
    return add_entry(&ent);
    case TOK_WRITEREGSTR:
    case TOK_WRITEREGEXPANDSTR:
    case TOK_WRITEREGBIN:
    case TOK_WRITEREGDWORD:
      {
        int k=line.gettoken_enum(1,rootkeys[0]);
        if (k == -1) k=line.gettoken_enum(1,rootkeys[1]);
        if (k == -1) PRINTHELP()
        ent.which=EW_WRITEREG;
        ent.offsets[0]=(int)rootkey_tab[k];
        ent.offsets[1]=add_string(line.gettoken_str(2));
        if (line.gettoken_str(2)[0] == '\\') warning("%s: registry path name begins with \'\\\', may cause problems (%s:%d)",line.gettoken_str(0),curfilename,linecnt);
        ent.offsets[2]=add_string(line.gettoken_str(3));
        if (which_token == TOK_WRITEREGSTR || which_token == TOK_WRITEREGEXPANDSTR)
        {
          SCRIPT_MSG("%s: %s\\%s\\%s=%s\n",
            line.gettoken_str(0),line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3),line.gettoken_str(4));
          ent.offsets[3]=add_string(line.gettoken_str(4));
          if (which_token == TOK_WRITEREGEXPANDSTR)
          {
            ent.offsets[4]=0;
          }
          else ent.offsets[4]=1;
        }
        if (which_token == TOK_WRITEREGBIN) 
        {
          char data[NSIS_MAX_STRLEN];
          char *p=line.gettoken_str(4);
          int data_len=0;
          while (*p)
          {
            int c;
            int a,b;
            a=*p;
            if (a >= '0' && a <= '9') a-='0';
            else if (a >= 'a' && a <= 'f') a-='a'-10;
            else if (a >= 'A' && a <= 'F') a-='A'-10;
            else break;
            b=*++p;
            if (b >= '0' && b <= '9') b-='0';
            else if (b >= 'a' && b <= 'f') b-='a'-10;
            else if (b >= 'A' && b <= 'F') b-='A'-10;
            else break;
            p++;
            c=(a<<4)|b;
            if (data_len >= NSIS_MAX_STRLEN) 
            {
              ERROR_MSG("WriteRegBin: %d bytes of data exceeded\n",NSIS_MAX_STRLEN);
              return PS_ERROR;
            }
            data[data_len++]=c;
          }
          if (*p) PRINTHELP()
          SCRIPT_MSG("WriteRegBin: %s\\%s\\%s=%s\n",
            line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3),line.gettoken_str(4));
          ent.offsets[3]=add_data(data,data_len);
          if (ent.offsets[3] < 0) return PS_ERROR;
          ent.offsets[4]=3;
        }
        if (which_token == TOK_WRITEREGDWORD) 
        {
          ent.offsets[3]=add_string(line.gettoken_str(4));
          ent.offsets[4]=2;

          SCRIPT_MSG("WriteRegDWORD: %s\\%s\\%s=%s\n",
            line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3),line.gettoken_str(4));
        }
      }
    return add_entry(&ent);
    case TOK_ENUMREGKEY:
    case TOK_ENUMREGVAL:
      {
        ent.which=EW_REGENUM;
        ent.offsets[0]=line.gettoken_enum(1,usrvars);
        int k=line.gettoken_enum(2,rootkeys[0]);
        if (k == -1) k=line.gettoken_enum(2,rootkeys[1]);
        if (ent.offsets[0] < 0 || k == -1) PRINTHELP()
        ent.offsets[1]=(int)rootkey_tab[k];
        ent.offsets[2]=add_string(line.gettoken_str(3));
        ent.offsets[3]=add_string(line.gettoken_str(4));
        ent.offsets[4]=which_token == TOK_ENUMREGKEY;
        if (line.gettoken_str(3)[0] == '\\') warning("%s: registry path name begins with \'\\\', may cause problems (%s:%d)",line.gettoken_str(0),curfilename,linecnt);
        SCRIPT_MSG("%s %s %s\\%s\\%s\n",which_token == TOK_ENUMREGKEY ? "EnumRegKey" : "EnumRegValue", 
          line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3),line.gettoken_str(4));
      }
    return add_entry(&ent);
#else//!NSIS_SUPPORT_REGISTRYFUNCTIONS
    case TOK_READREGSTR:
    case TOK_READREGDWORD:
    case TOK_DELETEREGVALUE:
    case TOK_DELETEREGKEY:
    case TOK_WRITEREGSTR:
    case TOK_WRITEREGEXPANDSTR:
    case TOK_WRITEREGBIN:
    case TOK_WRITEREGDWORD:
    case TOK_ENUMREGKEY:
    case TOK_ENUMREGVAL:
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_REGISTRYFUNCTIONS not defined.\n",  line.gettoken_str(0));
      return PS_ERROR;
#endif//!NSIS_SUPPORT_REGISTRYFUNCTIONS
#ifdef NSIS_SUPPORT_STACK
    case TOK_EXCH:
      {
        int swapitem=1;
        int save=line.gettoken_enum(1,usrvars);
        ent.which=EW_PUSHPOP;
        if (line.gettoken_str(1)[0] && save<0)
        {
          int s=0;
          swapitem=line.gettoken_int(1,&s);
          if (!s || swapitem <= 0) PRINTHELP()
        }
        if (save>=0)
        {
          SCRIPT_MSG("Exch(%s,0)\n",line.gettoken_str(1));
          ent.offsets[0]=add_string(line.gettoken_str(1));
          ent.offsets[1]=0;
          ent.offsets[2]=0;
          add_entry(&ent);
        }
        else SCRIPT_MSG("Exch(st(%d),0)\n",swapitem);

        ent.offsets[0]=0;
        ent.offsets[1]=0;
        ent.offsets[2]=swapitem;

        if (save>=0)
        {
          add_entry(&ent);
          ent.offsets[0]=save;
          ent.offsets[1]=1;
          ent.offsets[2]=0;
        }
      }
    return add_entry(&ent);
    case TOK_PUSH:
      ent.which=EW_PUSHPOP;
      ent.offsets[0]=add_string(line.gettoken_str(1));
      ent.offsets[1]=0;
      SCRIPT_MSG("Push: %s\n",line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_POP:
      ent.which=EW_PUSHPOP;
      ent.offsets[0]=line.gettoken_enum(1,usrvars);
      ent.offsets[1]=1;
      if (ent.offsets[0] < 0) PRINTHELP()
      SCRIPT_MSG("Pop: %s\n",line.gettoken_str(1));
    return add_entry(&ent);
#else//!NSIS_SUPPORT_STACK
    case TOK_POP:
    case TOK_PUSH:
    case TOK_EXCH:
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_STACK not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_SUPPORT_STACK
#ifdef NSIS_SUPPORT_ENVIRONMENT
    case TOK_READENVSTR:
      ent.which=EW_READENVSTR;
      ent.offsets[0]=line.gettoken_enum(1,usrvars);
      {
        ent.offsets[1]=add_string(line.gettoken_str(2));
        if (ent.offsets[0] < 0 || strlen(line.gettoken_str(2))<1) PRINTHELP()
      }
      ent.offsets[2]=1;
      SCRIPT_MSG("ReadEnvStr: %s->%s\n",line.gettoken_str(2),line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_EXPANDENVSTRS:
      ent.which=EW_READENVSTR;
      ent.offsets[0]=line.gettoken_enum(1,usrvars);
      ent.offsets[1]=add_string(line.gettoken_str(2));
      ent.offsets[2]=0;
      if (ent.offsets[0] < 0) PRINTHELP()
      SCRIPT_MSG("ExpandEnvStrings: %s->%s\n",line.gettoken_str(2),line.gettoken_str(1));
    return add_entry(&ent);
#else//!NSIS_SUPPORT_ENVIRONMENT
    case TOK_EXPANDENVSTRS:
    case TOK_READENVSTR:
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_ENVIRONMENT not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_SUPPORT_ENVIRONMENT
#ifdef NSIS_SUPPORT_FINDFIRST
    case TOK_FINDFIRST:
      ent.which=EW_FINDFIRST;
      ent.offsets[0]=add_string(line.gettoken_str(3)); // filespec
      ent.offsets[1]=line.gettoken_enum(2,usrvars); // out
      ent.offsets[2]=line.gettoken_enum(1,usrvars); // handleout
      if (ent.offsets[1] < 0 || ent.offsets[2] < 0) PRINTHELP()
      SCRIPT_MSG("FindFirst: spec=\"%s\" handle=%s output=%s\n",line.gettoken_str(3),line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);
    case TOK_FINDNEXT:
      ent.which=EW_FINDNEXT;
      ent.offsets[0]=line.gettoken_enum(2,usrvars);
      ent.offsets[1]=line.gettoken_enum(1,usrvars);
      if (ent.offsets[0] < 0 || ent.offsets[1] < 0) PRINTHELP()
      SCRIPT_MSG("FindNext: handle=%s output=%s\n",line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);
    case TOK_FINDCLOSE:
      ent.which=EW_FINDCLOSE;
      ent.offsets[0]=line.gettoken_enum(1,usrvars);
      if (ent.offsets[0] < 0) PRINTHELP()
      SCRIPT_MSG("FindClose: %s\n",line.gettoken_str(1));
    return add_entry(&ent);
#else//!NSIS_SUPPORT_FINDFIRST
    case TOK_FINDCLOSE:
    case TOK_FINDNEXT:
    case TOK_FINDFIRST:
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_FINDFIRST not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;

#endif//!NSIS_SUPPORT_FINDFIRST


#ifdef NSIS_SUPPORT_FILEFUNCTIONS
    case TOK_FILEOPEN:
      {
        ent.which=EW_FOPEN;
        ent.offsets[3]=line.gettoken_enum(1,usrvars); // file handle
        ent.offsets[0]=add_string(line.gettoken_str(2));
        ent.offsets[1]=0; //openmode
        if (!stricmp(line.gettoken_str(3),"r"))
        {
          ent.offsets[1]=GENERIC_READ;
          ent.offsets[2]=OPEN_EXISTING;
        }
        else if (!stricmp(line.gettoken_str(3),"w"))
        {
          ent.offsets[1]=GENERIC_WRITE;
          ent.offsets[2]=CREATE_ALWAYS;
        }
        else if (!stricmp(line.gettoken_str(3),"a"))
        {
          ent.offsets[1]=GENERIC_WRITE|GENERIC_READ;
          ent.offsets[2]=OPEN_ALWAYS;
        }
      
        if (ent.offsets[3] < 0 || !ent.offsets[1]) PRINTHELP()
      }
      SCRIPT_MSG("FileOpen: %s as %s -> %s\n",line.gettoken_str(2),line.gettoken_str(3),line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_FILECLOSE:
      ent.which=EW_FCLOSE;
      ent.offsets[0]=line.gettoken_enum(1,usrvars); // file handle
      if (ent.offsets[0] < 0) PRINTHELP()
      SCRIPT_MSG("FileClose: %s\n",line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_FILEREAD:
      ent.which=EW_FGETS;
      ent.offsets[0]=line.gettoken_enum(1,usrvars); // file handle
      ent.offsets[1]=line.gettoken_enum(2,usrvars); // output string
      ent.offsets[2]=add_string(line.gettoken_str(3)[0]?line.gettoken_str(3):"1023");
      if (ent.offsets[0]<0 || ent.offsets[1]<0) PRINTHELP()
      SCRIPT_MSG("FileRead: %s->%s (max:%s)\n",line.gettoken_str(1),line.gettoken_str(2),line.gettoken_str(3));
    return add_entry(&ent);
    case TOK_FILEWRITE:
      ent.which=EW_FPUTS;
      ent.offsets[0]=line.gettoken_enum(1,usrvars); // file handle
      ent.offsets[1]=add_string(line.gettoken_str(2));
      if (ent.offsets[0]<0) PRINTHELP()
      SCRIPT_MSG("FileWrite: %s->%s\n",line.gettoken_str(2),line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_FILEREADBYTE:
      ent.which=EW_FGETS;
      ent.offsets[0]=line.gettoken_enum(1,usrvars); // file handle
      ent.offsets[1]=line.gettoken_enum(2,usrvars); // output string
      ent.offsets[2]=add_string("1");
      ent.offsets[3]=1;
      if (ent.offsets[0]<0 || ent.offsets[1]<0) PRINTHELP()
      SCRIPT_MSG("FileReadByte: %s->%s\n",line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);
    case TOK_FILEWRITEBYTE:
      ent.which=EW_FPUTS;
      ent.offsets[0]=line.gettoken_enum(1,usrvars); // file handle
      ent.offsets[1]=add_string(line.gettoken_str(2));
      ent.offsets[2]=1;
      if (ent.offsets[0]<0) PRINTHELP()
      SCRIPT_MSG("FileWriteByte: %s->%s\n",line.gettoken_str(2),line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_FILESEEK:
      {
        char *modestr;
        int tab[3]={FILE_BEGIN,FILE_CURRENT,FILE_END};
        int mode=line.gettoken_enum(3,"SET\0CUR\0END\0");
        ent.which=EW_FSEEK;
        ent.offsets[0]=line.gettoken_enum(1,usrvars);
        ent.offsets[1]=add_string(line.gettoken_str(2));
        ent.offsets[3]=line.gettoken_enum(4,usrvars);

        if (mode<0 && !line.gettoken_str(3)[0]) 
        {
          mode=0;
          modestr="SET";
        }
        else modestr=line.gettoken_str(3);

        if (mode<0 || ent.offsets[0] < 0 || (ent.offsets[3]<0 && line.gettoken_str(4)[0])) PRINTHELP()
        ent.offsets[2]=tab[mode];
        SCRIPT_MSG("FileSeek: fp=%s, ofs=%s, mode=%s, output=%s\n",
          line.gettoken_str(1),
          line.gettoken_str(2),
          modestr,
          line.gettoken_str(4));
      }

    return add_entry(&ent);
#else//!NSIS_SUPPORT_FILEFUNCTIONS
    case TOK_FILEOPEN:
    case TOK_FILECLOSE:
    case TOK_FILESEEK:
    case TOK_FILEREAD:
    case TOK_FILEWRITE:
    case TOK_FILEREADBYTE:
    case TOK_FILEWRITEBYTE:
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_FILEFUNCTIONS not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;

#endif//!NSIS_SUPPORT_FILEFUNCTIONS
#ifdef NSIS_SUPPORT_REBOOT
    case TOK_REBOOT:
      ent.which=EW_REBOOT;
      ent.offsets[0]=0xbadf00d;
      SCRIPT_MSG("Reboot! (WOW)\n");
    return add_entry(&ent);
    case TOK_IFREBOOTFLAG:
      ent.which=EW_IFREBOOTFLAG;
      if (process_jump(line,1,&ent.offsets[0]) || 
          process_jump(line,2,&ent.offsets[1])) PRINTHELP()
      SCRIPT_MSG("IfRebootFlag ?%s:%s\n",line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);
    case TOK_SETREBOOTFLAG:
      ent.which=EW_SETREBOOTFLAG;
      ent.offsets[0]=line.gettoken_enum(1,"false\0true\0");
      if (ent.offsets[0] < 0) PRINTHELP()
    return add_entry(&ent);
#else//!NSIS_SUPPORT_REBOOT
    case TOK_REBOOT:
    case TOK_IFREBOOTFLAG:
    case TOK_SETREBOOTFLAG:
      ERROR_MSG("Error: %s specified, NSIS_SUPPORT_REBOOT not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_SUPPORT_REBOOT
#ifdef NSIS_CONFIG_LOG
    case TOK_LOGSET:
      ent.which=EW_LOG;
      ent.offsets[0]=1;
      ent.offsets[1]=line.gettoken_enum(1,"off\0on\0");
      if (ent.offsets[1]<0) PRINTHELP()
      
      SCRIPT_MSG("LogSet: %s\n",line.gettoken_str(1));
    return add_entry(&ent);
    case TOK_LOGTEXT:
      ent.which=EW_LOG;
      ent.offsets[0]=0;
      ent.offsets[1]=add_string(line.gettoken_str(1));
      SCRIPT_MSG("LogText \"%s\"\n",line.gettoken_str(1));
    return add_entry(&ent);
#else//!NSIS_CONFIG_LOG

    case TOK_LOGSET:
    case TOK_LOGTEXT:
      ERROR_MSG("Error: %s specified, NSIS_CONFIG_LOG not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_CONFIG_LOG
#ifdef NSIS_CONFIG_COMPONENTPAGE
    case TOK_SECTIONSETTEXT:
      if (uninstall_mode)
      {
        ERROR_MSG("Error: %s called in uninstall section.\n",  line.gettoken_str(0));
        return PS_ERROR;
      }
      ent.which=EW_SECTIONSET;
      ent.offsets[0]=add_string(line.gettoken_str(1));
      ent.offsets[1]=0;
      ent.offsets[2]=add_string(line.gettoken_str(2));
      SCRIPT_MSG("SectionSetText: %s=%s\n",line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);
    case TOK_SECTIONGETTEXT:
      if (uninstall_mode)
      {
        ERROR_MSG("Error: %s called in uninstall section.\n",  line.gettoken_str(0));
        return PS_ERROR;
      }
      ent.which=EW_SECTIONSET;
      ent.offsets[0]=add_string(line.gettoken_str(1));
      ent.offsets[1]=1;
      ent.offsets[2]=line.gettoken_enum(2,usrvars);
      if (line.gettoken_str(2)[0] && ent.offsets[2]<0) PRINTHELP()
      SCRIPT_MSG("SectionGetText: %s->%s\n",line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);    
    case TOK_SECTIONSETFLAGS:
      if (uninstall_mode)
      {
        ERROR_MSG("Error: %s called in uninstall section.\n",  line.gettoken_str(0));
        return PS_ERROR;
      }
      ent.which=EW_SECTIONSET;
      ent.offsets[0]=add_string(line.gettoken_str(1));
      ent.offsets[1]=2;
      ent.offsets[2]=add_string(line.gettoken_str(2));
      SCRIPT_MSG("SectionSetFlags: %s->%s\n",line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);
    case TOK_SECTIONGETFLAGS:
      if (uninstall_mode)
      {
        ERROR_MSG("Error: %s called in uninstall section.\n",  line.gettoken_str(0));
        return PS_ERROR;
      }
      ent.which=EW_SECTIONSET;
      ent.offsets[0]=add_string(line.gettoken_str(1));
      ent.offsets[1]=3;
      ent.offsets[2]=line.gettoken_enum(2,usrvars);
      if (line.gettoken_str(2)[0] && ent.offsets[2]<0) PRINTHELP()
      SCRIPT_MSG("SectionGetFlags: %s->%s\n",line.gettoken_str(1),line.gettoken_str(2));
    return add_entry(&ent);
#else//!NSIS_CONFIG_COMPONENTPAGE
    case TOK_SECTIONGETTEXT:
    case TOK_SECTIONSETTEXT:
    case TOK_SECTIONSETFLAGS:
    case TOK_SECTIONGETFLAGS:
      ERROR_MSG("Error: %s specified, NSIS_CONFIG_COMPONENTPAGE not defined.\n",  line.gettoken_str(0));
    return PS_ERROR;
#endif//!NSIS_CONFIG_COMPONENTPAGE
    // Added by Amir Szekely 29th July 2002
    case TOK_SETBRANDINGIMAGE:
#ifdef NSIS_CONFIG_VISIBLE_SUPPORT
    {
      SCRIPT_MSG("SetBrandingImage: %s\n", line.gettoken_str(1));
      if (!branding_image_found) {
        ERROR_MSG("Error: no branding image found in choosen UI!\n");
        return PS_ERROR;
      }
      ent.which=EW_SETBRANDINGIMAGE;
      for (int i = 1; i < line.getnumtokens(); i++)
        if (!strnicmp(line.gettoken_str(i),"/IMGID=",7))
          ent.offsets[1]=atoi(line.gettoken_str(i)+7);
        else if (!stricmp(line.gettoken_str(i),"/RESIZETOFIT"))
          ent.offsets[2]=1;
        else if (!ent.offsets[0])
          ent.offsets[0]=add_string(line.gettoken_str(i));
        else
          PRINTHELP();

      if (!ent.offsets[1])
        ent.offsets[1]=branding_image_id;
    }
    return add_entry(&ent);
#else
    ERROR_MSG("Error: %s specified, NSIS_CONFIG_VISIBLE_SUPPORT not defined.\n",line.gettoken_str(0));
    return PS_ERROR;
#endif// NSIS_CONFIG_VISIBLE_SUPPORT

    // end of instructions
    ///////////////////////////////////////////////////////////////////////////////

    default: break;

  }
  ERROR_MSG("Error: doCommand: Invalid token \"%s\".\n",line.gettoken_str(0));
  return PS_ERROR;
}

int CEXEBuild::do_add_file(const char *lgss, int attrib, int recurse, int linecnt, int *total_files, const char *name_override)
{
  char dir[1024];
  char newfn[1024], *s;
  HANDLE h;
  WIN32_FIND_DATA d;
  strcpy(dir,lgss);
  s=dir+strlen(dir);
  while (s > dir && *s != '\\') s=CharPrev(dir,s);
  *s=0;

  h = FindFirstFile(lgss,&d);
  if (h != INVALID_HANDLE_VALUE)
  {
    do 
    {
      if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
      {
        if (recurse && strcmp(d.cFileName,"..") && strcmp(d.cFileName,".")) 
        {
          entry ent={0,};
          int a;
          int wd_save=strlen(cur_out_path);

          {
            char *i=d.cFileName,*o=cur_out_path;
            while (*o) o++;
            if (o > cur_out_path && CharPrev(cur_out_path,o)[0] != '\\') *o++='\\';

            while (*i)
            {
              char *ni=CharNext(i);
              if (ni-i > 1)
              {
                int l=ni-i;
                while (l--)
                {
                  *o++=*i++;
                }
              }
              else
              {
                char c=*i++;
                *o++=c;
                if (c == '$') *o++='$';
              }
            }
            *o=0;
          }

          (*total_files)++;
          ent.which=EW_CREATEDIR;
          ent.offsets[0]=add_string(cur_out_path);
          ent.offsets[1]=1;
          a=add_entry(&ent);
		  if (a != PS_OK) 
          {
            FindClose(h);
            return a;
          }
          if (attrib)
          {
            ent.which=EW_SETFILEATTRIBUTES;		
            ent.offsets[0]=add_string(cur_out_path);
            ent.offsets[1]=d.dwFileAttributes;

            a=add_entry(&ent);
            if (a != PS_OK) 
            {
              FindClose(h);
              return a;
            }
          }
          char spec[1024];
          sprintf(spec,"%s%s%s",dir,dir[0]?"\\":"",d.cFileName);
          SCRIPT_MSG("File: Descending to: \"%s\" -> \"%s\"\n",spec,cur_out_path);
          strcat(spec,"\\*.*");
          a=do_add_file(spec,attrib,recurse,linecnt,total_files);
          if (a != PS_OK)
          {
            FindClose(h);
            return a;
          }

          cur_out_path[wd_save]=0;
          ent.which=EW_CREATEDIR;
          ent.offsets[1]=1;
          SCRIPT_MSG("File: Returning to: \"%s\" -> \"%s\"\n",dir,cur_out_path);
          ent.offsets[0]=add_string(cur_out_path);
          a=add_entry(&ent);
          if (a != PS_OK) 
          {
            FindClose(h);
            return a;
          }
        }
      }
      else
      {
        HANDLE hFile,hFileMap;
        DWORD len;
        (*total_files)++;
        sprintf(newfn,"%s%s%s",dir,dir[0]?"\\":"",d.cFileName);
        hFile=CreateFile(newfn,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
          ERROR_MSG("File: failed opening file \"%s\"\n",newfn);
          return PS_ERROR;
        }
        hFileMap=NULL;
        len = GetFileSize(hFile, NULL);
        if (len && !(hFileMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL)))
        {
          CloseHandle(hFile);
          ERROR_MSG("File: failed creating mmap of \"%s\"\n",newfn);
          return PS_ERROR;
        }
        char *filedata=NULL;
        if (len)
        {
          filedata=(char*)MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0);
          if (!filedata)
          {
            if (hFileMap) CloseHandle(hFileMap);
            CloseHandle(hFile);
            ERROR_MSG("File: failed mmapping file \"%s\"\n",newfn);
            return PS_ERROR;
          }
        }

        section_add_size_kb((len+1023)/1024);
        if (name_override) SCRIPT_MSG("File: \"%s\"->\"%s\"",d.cFileName,name_override);
        else SCRIPT_MSG("File: \"%s\"",d.cFileName);
        if (!build_compress_whole)
          if (build_compress) SCRIPT_MSG(" [compress]");
        fflush(stdout);
		char buf[1024];
        int last_build_datablock_used=getcurdbsize();
        entry ent={0,};
        ent.which=EW_EXTRACTFILE;
        ent.offsets[0]=build_overwrite;
        if (name_override)
        {
          ent.offsets[1]=add_string(name_override);
        }
        else
        {
          char *i=d.cFileName,*o=buf;
          while (*i)
          {
            char c=*i++;
            *o++=c;
            if (c == '$') *o++='$';
          }
          *o=0;
          ent.offsets[1]=add_string(buf);
        }
        ent.offsets[2]=add_data(filedata?filedata:"",len);

        if (filedata) UnmapViewOfFile(filedata);
        if (hFileMap) CloseHandle(hFileMap);

        if (ent.offsets[2] < 0) 
        {
          CloseHandle(hFile);
          return PS_ERROR;
        }

        {
          DWORD s=getcurdbsize()-last_build_datablock_used;
          if (s) s-=4;
          if (s != len) SCRIPT_MSG(" %d/%d bytes\n",s,len);
          else SCRIPT_MSG(" %d bytes\n",len);
        }

        if (build_datesave || build_overwrite==0x3 /*ifnewer*/)
        {
          FILETIME ft;
          if (GetFileTime(hFile,NULL,NULL,&ft))
          {
            ent.offsets[3]=ft.dwLowDateTime;
            ent.offsets[4]=ft.dwHighDateTime;
          }
          else
          {
            CloseHandle(hFile);
            ERROR_MSG("File: failed getting file date from \"%s\"\n",newfn);
            return PS_ERROR;
          }
        }
        else
        {
          ent.offsets[3]=0xffffffff;
          ent.offsets[4]=0xffffffff;
        }
        if (uninstall_mode) m_uninst_fileused++;
        else m_inst_fileused++;

        CloseHandle(hFile);
        int a=add_entry(&ent);
        if (a != PS_OK) 
        {
          FindClose(h);
          return a;
        }
        if (attrib)
        {
          char tmp_path[1024];
          ent.which=EW_SETFILEATTRIBUTES;		
          if (name_override)
          {
            sprintf(tmp_path,"%s\\%s",cur_out_path,name_override);
          }
          else
          {
            sprintf(tmp_path,"%s\\%s",cur_out_path,buf);
          }
          ent.offsets[0]=add_string(tmp_path);
          ent.offsets[1]=d.dwFileAttributes;

          a=add_entry(&ent);
          if (a != PS_OK) 
          {
            FindClose(h);
            return a;
          }
         }
      }
    } while (FindNextFile(h,&d));
    FindClose(h);
  }
  return PS_OK;
}
  
