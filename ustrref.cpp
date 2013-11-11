///////////////////////////////////////////////////////////////////////////////
//
//  FileName    :   ustrref.cpp
//  Version     :   0.11
//  Author      :   Luo Cong
//  Date        :   2004-07-27 (yyyy-mm-dd)
//  Comment     :   中文搜索引擎
//
///////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <stdio.h>
#include "Plugin.h"

#include <string>
#include <vector>
using namespace std;

#define VERSIONY   2012
#define VERSIONM   4
#define VERSIOND   22

#define CHECKMAX   13
#define ABLEMAX    10
#define WSTRLEN        100

static HINSTANCE    g_hInstance             =   NULL;
static HWND         g_hWndMain              =   NULL;
static char         g_szIniKey[]            =   "Restore UStrRef Window";
static char         g_szPluginName[]        =   "中文搜索引擎";
static char         g_szPassedTheEndOfFile[]=   "Passed the end of file";
static char         g_szTextToFind[TEXTLEN] =   { 0 };
static int          g_CurEIP_Item_Index     =   0;
vector<string>      g_strings;
static char         g_szustrrefclass[32];
static t_table      g_ustrreftbl;
static unsigned char g_chrRet[2];


typedef struct t_ustrref
{
    ulong           index;              // ustrref index
    ulong           size;               // Size of index, always 1 in our case
    ulong           type;               // Type of entry, always 0
    ulong           addr;               // Address of string
    int             iscureip;           // Is Current EIP? 0: No, 1: Yes.
} t_ustrref;

BOOL APIENTRY DllMain(
					  HINSTANCE hModule,
					  DWORD  dwReason,
					  LPVOID lpReserved
					  )
{
    if (DLL_PROCESS_ATTACH == dwReason)
        g_hInstance = hModule;
    return TRUE;
}

int cdecl UStrRefGetText(
						 char *s,
						 char *mask,
						 int *select,
						 t_sortheader *ph,
						 int column
						 )
{
    t_disasm      da;
    int           n = 0;
    t_memory      *pmem;
    ulong         cmdsize;
    uchar         *pdecode;
    ulong         decodesize;
    unsigned char cmd[MAXCMDSIZE];
    t_ustrref     *pb = (t_ustrref *)ph;
	
    if (pb->iscureip)   // Is Current EIP
        *select = DRAW_HILITE;
    else
        *select = NULL;
	
    switch (column)
    {
    case 0: // Address
        n = sprintf((char *)s, "%08X", pb->addr);
        break;
    case 1: // Disassembly
        pmem = Findmemory(pb->addr);
        if (NULL == pmem)
        {
            *select = DRAW_GRAY;
            return sprintf((char *)s, "???");
        }
        cmdsize = pmem->base + pmem->size - pb->addr;
        if (cmdsize > MAXCMDSIZE)
            cmdsize = MAXCMDSIZE;
        if (
            cmdsize != Readmemory(cmd, pb->addr, cmdsize, MM_RESTORE|MM_SILENT)
			)
        {
            *select = DRAW_GRAY;
            return sprintf((char *)s, "???");
        }
        pdecode = Finddecode(pb->addr, &decodesize);
        if (decodesize < cmdsize)
            pdecode = NULL;
        Disasm(cmd, cmdsize, pb->addr, pdecode, &da, DISASM_CODE, 0);
        n = sprintf((char *)s, "%s", da.result);
        break;
    case 2: // Text String
        n = sprintf((char *)s, "%s", g_strings[pb->index].c_str());
        break;
    }
    return n;
}

int FindNextAndScroll(HWND hWnd, int nStartPos)
{
    int i;
    int found = 0;
    char *pdest = NULL;
    string strinarray;
    char texttofind[TEXTLEN];
    
    strcpy(texttofind, g_szTextToFind);
	
    for (i = nStartPos; i < g_strings.size(); ++i)
    {
        strinarray = g_strings[i].c_str();
        pdest = strstr(
            strlwr((char *)strinarray.c_str()),
            strlwr(texttofind)
			);
        if (NULL != pdest)
        {
            Selectandscroll(&g_ustrreftbl, i, 2);
            InvalidateRect(hWnd, NULL, FALSE);
            found = 1;
            break;
        }
    }
    return found;
}

int FindPreviousAndScroll(HWND hWnd, int nStartPos)
{
    int i;
    int found = 0;
    char *pdest = NULL;
    string strinarray;
    char texttofind[TEXTLEN];
    
    strcpy(texttofind, g_szTextToFind);
	
    for (i = nStartPos; i >= 0; --i)
    {
        strinarray = g_strings[i].c_str();
        pdest = strstr(
            strlwr((char *)strinarray.c_str()),
            strlwr(texttofind)
			);
        if (NULL != pdest)
        {
            Selectandscroll(&g_ustrreftbl, i, 2);
            InvalidateRect(hWnd, NULL, FALSE);
            found = 1;
            break;
        }
    }
    return found;
}

void FindPrevious(HWND hWnd)
{
    int found = 0;
    t_ustrref *pb;
	
    pb = (t_ustrref *)Getsortedbyselection(
        &(g_ustrreftbl.data), g_ustrreftbl.data.selected);
    if (NULL != pb && '\0' != g_szTextToFind[0])
    {
        found = FindPreviousAndScroll(hWnd, pb->index - 1);
        // if not found, scroll to the last index and find again
        if (!found)
        {
            Flash(g_szPassedTheEndOfFile);
            FindPreviousAndScroll(hWnd, g_strings.size() - 1);
        }
    }
}

void FindNext(HWND hWnd)
{
    int found = 0;
    t_ustrref *pb;
	
    pb = (t_ustrref *)Getsortedbyselection(
        &(g_ustrreftbl.data), g_ustrreftbl.data.selected);
    if (NULL != pb && '\0' != g_szTextToFind[0])
    {
        found = FindNextAndScroll(hWnd, pb->index + 1);
        // if not found, scroll to index 0 and find again
        if (!found)
        {
            Flash(g_szPassedTheEndOfFile);
            FindNextAndScroll(hWnd, 0);
        }
    }
}

void Find(HWND hWnd)
{
    int  found = 0;
    t_ustrref *pb;
    char textnotfound[20 + TEXTLEN];
	
    Gettext("Find", g_szTextToFind, 0, NM_COMMENT, FIXEDFONT);
	
    pb = (t_ustrref *)Getsortedbyselection(
        &(g_ustrreftbl.data), g_ustrreftbl.data.selected);
	
    if (NULL != pb && '\0' != g_szTextToFind[0])
    {
        // find from current selected index
        found = FindNextAndScroll(hWnd, pb->index);
        // if not found, scroll to index 0 and find again
        if (!found)
        {
            found = FindNextAndScroll(hWnd, 0);
            if (!found)
            {
                sprintf(textnotfound, "\"%s\" not found!", g_szTextToFind);
                MessageBox(
                    hWnd,
                    textnotfound,
                    g_szPluginName,
                    MB_OK | MB_ICONINFORMATION
					);
            }
        }
    }
}

LRESULT CALLBACK UStrRefWndProc(
								HWND hWnd,
								UINT msg,
								WPARAM wParam,
								LPARAM lParam
								)
{
    int         i;
    t_ustrref   *pb;
    HMENU       menu;
	
    switch (msg)
    {
		// Standard messages. You can process them, but - unless absolutely sure -
		// always pass them to Tablefunction().
    case WM_DESTROY:
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_HSCROLL:
    case WM_VSCROLL:
    case WM_TIMER:
    case WM_SYSKEYDOWN:
        Tablefunction(&g_ustrreftbl, hWnd, msg, wParam, lParam);
        break;                           // Pass message to DefMDIChildProc()
		// Custom messages responsible for scrolling and selection. User-drawn
		// windows must process them, standard OllyDbg windows without extra
		// functionality pass them to Tablefunction().
    case WM_USER_SCR:
    case WM_USER_VABS:
    case WM_USER_VREL:
    case WM_USER_VBYTE:
    case WM_USER_STS:
    case WM_USER_CNTS:
    case WM_USER_CHGS:
        return Tablefunction(&g_ustrreftbl, hWnd, msg, wParam, lParam);
		// WM_WINDOWPOSCHANGED is responsible for always-on-top MDI windows.
    case WM_WINDOWPOSCHANGED:
        return Tablefunction(&g_ustrreftbl, hWnd, msg, wParam, lParam);
    case WM_USER_MENU:
        menu = CreatePopupMenu();
        // Find selected string. Any operations with string make sense only
        // if at least one string exists and is selected. Note that sorted data
        // has special sort index table which is updated only when necessary.
        // Getsortedbyselection() does this; some other sorted data functions
        // don't and you must call Sortsorteddata().
        pb = (t_ustrref *)Getsortedbyselection(
			&(g_ustrreftbl.data), g_ustrreftbl.data.selected);
        if (NULL != menu && NULL != pb)
        {
            AppendMenu(menu, MF_STRING, 1, "&Follow\tEnter");
            AppendMenu(menu, MF_SEPARATOR, NULL, NULL);
            AppendMenu(menu, MF_STRING, 2, "F&ind\tCtrl+F or Insert");
            AppendMenu(menu, MF_STRING, 3, "Find &Next\tb or n");
            AppendMenu(menu, MF_STRING, 4, "Find &Previous\tz or p");
        }
        // Even when menu is NULL, call Tablefunction is still meaningful.
        i = Tablefunction(
            &g_ustrreftbl,
            hWnd,
            WM_USER_MENU,
            0,
            (LPARAM)menu
			);
        if (NULL != menu)
            DestroyMenu(menu);
        if (1 == i)                 // Follow address in Disassembler
        {
            Setcpu(
                0,
                pb->addr,
                0,
                0,
                CPU_ASMHIST | CPU_ASMCENTER | CPU_ASMFOCUS
				);
        }
        else if (2 == i)            // Find
        {
            Find(hWnd);
        }
        else if (3 == i)            // Find next
        {
            FindNext(hWnd);
        }
        return 0;
    case WM_KEYDOWN:
        // Processing of WM_KEYDOWN messages is - surprise, surprise - very
        // similar to that of corresponding menu entries.
        switch (wParam)
        {
        case VK_RETURN:
            // Return key follows address in Disassembler.
            pb = (t_ustrref *)Getsortedbyselection(
                &(g_ustrreftbl.data), g_ustrreftbl.data.selected);
            if (NULL != pb)
            {
                Setcpu(
                    0,
                    pb->addr,
                    0,
                    0,
                    CPU_ASMHIST | CPU_ASMCENTER | CPU_ASMFOCUS
					);
            }
            break;
        case VK_INSERT:
            Find(hWnd);
            break;
        case 0x46:  // Ctrl+'F' or Ctrl+'f'
            if (GetKeyState(VK_CONTROL) < 0)    // Ctrl key is pressing
                Find(hWnd);
            break;
        case 0x42:  // VK_B
        case 0x4e:  // VK_N
            FindNext(hWnd);
            break;
        case 0x5a:  // VK_Z
        case 0x50:  // VK_P
            FindPrevious(hWnd);
            break;
        default:
            // Add all this arrow, home and pageup functionality.
            Tablefunction(&g_ustrreftbl, hWnd, msg, wParam, lParam);
            break;
        }
        break;
		case WM_USER_DBLCLK:
			// Doubleclicking row follows address in Disassembler.
			pb = (t_ustrref *)Getsortedbyselection(
				&(g_ustrreftbl.data), g_ustrreftbl.data.selected);
			if (NULL != pb)
			{
				Setcpu(
					0,
					pb->addr,
					0,
					0,
					CPU_ASMHIST | CPU_ASMCENTER | CPU_ASMFOCUS
					);
			}
			return 1;                        // Doubleclick processed
		case WM_USER_CHALL:
		case WM_USER_CHMEM:
			// Something is changed, redraw window.
			InvalidateRect(hWnd, NULL, FALSE);
			return 0;
		case WM_PAINT:
			// Fill background of search window with default button colour.
			// Necessary because Registerpluginclass() sets hbrBackground to NULL.
			
			// Painting of all OllyDbg windows is done by Painttable(). Make custom
			// drawing only if you have important reasons to do this.
			Painttable(hWnd, &g_ustrreftbl, UStrRefGetText);
			return 0;
		default:
			break;
    }
	
    return DefMDIChildProc(hWnd, msg, wParam, lParam);
}

void CreateUStrRefWindow(void)
{
    if (0 == g_ustrreftbl.bar.nbar)
    {
        g_ustrreftbl.bar.name[0]    = "地址";
        g_ustrreftbl.bar.defdx[0]   = 9;
        g_ustrreftbl.bar.mode[0]    = 0;
        g_ustrreftbl.bar.name[1]    = "反汇编";
        g_ustrreftbl.bar.defdx[1]   = 40;
        g_ustrreftbl.bar.mode[1]    = BAR_NOSORT;
        g_ustrreftbl.bar.name[2]    = "文本字符串";
        g_ustrreftbl.bar.defdx[2]   = 256;
        g_ustrreftbl.bar.mode[2]    = BAR_NOSORT;
        g_ustrreftbl.bar.nbar       = 3;
        g_ustrreftbl.mode =
            TABLE_COPYMENU |
            TABLE_SORTMENU |
            TABLE_APPMENU  |
            TABLE_SAVEPOS  |
            TABLE_ONTOP;
        g_ustrreftbl.drawfunc = UStrRefGetText;
    }
	
    Quicktablewindow(
        &g_ustrreftbl,
        15,
        3,
        g_szustrrefclass,
        g_szPluginName
		);
}

int UStrRefSortFunc(t_ustrref *b1, t_ustrref *b2, int sort)
{
    int i = 0;
	
    if (1 == sort)
    {
        // Sort by address
        if (b1->addr < b2->addr)
            i = -1;
        else if (b1->addr > b2->addr)
            i = 1;
    };
    // If elements are equal or sorting is by the first column, sort by index.
    if (0 == i)
    {
        if (b1->index < b2->index)
            i = -1;
        else if (b1->index > b2->index)
            i = 1;
    };
    return i;
}

extc int _export cdecl ODBG_Plugindata(char shortname[32])
{
    strcpy(shortname, g_szPluginName);
    return PLUGIN_VERSION;
}

void UnicodeToGB2312(unsigned char* pOut,unsigned short uData)   
{   
    WideCharToMultiByte(CP_ACP,NULL,(LPCWSTR)&uData,1,(LPSTR)pOut,sizeof(unsigned short),NULL,NULL);   
    return;   
}
int IsSimplifiedCH2(unsigned short &dch)
{
    int nRet;
    unsigned char highbyte;
    unsigned char lowbyte;
	
    highbyte = dch & 0x00ff;
    lowbyte  = dch / 0x0100;
	
	if (highbyte==0 || lowbyte==0)
		return 0;
	
    //    区名    码位范围   码位数  字符数 字符类型
    // 双字节2区 B0A1—F7FE   6768    6763    汉字
    if ((highbyte >= 0xb0 && highbyte <= 0xf7) && (lowbyte  >= 0xa1 && lowbyte  <= 0xfe))
        return 1;
	
	char GBt[3];
	char GBs[3];
	memset(GBt,0,3);
	memcpy(GBt,&dch,2);
	memset(GBs,0,3);
	if (LCMapString(0x0804,LCMAP_SIMPLIFIED_CHINESE, GBt, 2, GBs, 2))
	{
		highbyte=GBs[0];
		lowbyte=GBs[1];
		if (highbyte==0 || lowbyte==0)
			return 0;
		if ((highbyte >= 0xb0 && highbyte <= 0xf7) && (lowbyte  >= 0xa1 && lowbyte  <= 0xfe))
		{
			memcpy(&dch,GBs,2);
			return 1;
		}
	}
	
    return 0;
}
int IsGraphicCH2(const unsigned short dch)
{
    int nRet;
    unsigned char highbyte;
    unsigned char lowbyte;
	
    highbyte = dch & 0x00ff;
    lowbyte  = dch / 0x0100;
	
    //    区名    码位范围   码位数  字符数 字符类型
    // 双字节1区 A1A1—A9FE    846     718  图形符号
    if (
        (highbyte >= 0xa1 && highbyte <= 0xa3) &&
        (lowbyte  >= 0xa1 && lowbyte  <= 0xfe)
		)
        nRet = 1;
    else
        nRet = 0;
	
    return nRet;
}
int IsStrW(DWORD StrBuf)
{
	return 1;
}
int IsStrA(DWORD StrBuf)
{
	return 1;
}
bool IsAlpha2(const unsigned short dch,unsigned char *g_chrRet)
{
	g_chrRet[0]=0;
	g_chrRet[1]=0;
	
    UnicodeToGB2312(g_chrRet,dch);
	
	if (!IsSimplifiedCH2(*(unsigned short *)g_chrRet) && !IsGraphicCH2(*(unsigned short *)g_chrRet))
		return false;
	
	if ((g_chrRet[0]==0 || g_chrRet[1]==0))
		return false;
	else
		return true;
}
int IsAscII2(const unsigned short dch)
{
    int nRet;
    unsigned char highbyte;
    unsigned char lowbyte;
	
    highbyte = dch & 0x00ff;
    lowbyte  = dch / 0x0100;
	
    if (highbyte>0x1B && highbyte<0x7F && lowbyte==0)
        nRet = 1;
    else
        nRet = 0;
	
    return nRet;
}
typedef int (__cdecl *RealStrToTextType) ( DWORD , DWORD , DWORD , DWORD );
RealStrToTextType RealStrToText;
bool isSimGra2(const unsigned short dch,unsigned char *g_chrRet)
{
	g_chrRet[0]=0;
	g_chrRet[1]=0;
	
	memcpy(g_chrRet,&dch,2);
	
	if (IsSimplifiedCH2(*(unsigned short *)g_chrRet) || IsGraphicCH2(*(unsigned short *)g_chrRet))
		return true;
	else
		return false;
}
int StrToTextA(DWORD Str,DWORD StrLen,DWORD Dest,DWORD MaxLen)
{
	string strAsc;
	
    int             i=0;
    int             len = 0;
    int             nInsert=0;
	unsigned char g_chrRet[2];
    string          stringitem;
	stringitem.erase();
	
	unsigned char * buf=(unsigned char *)Str;
	
	while (('\0' != *(buf + i)) && (i < WSTRLEN))
	{
		if ('\r' == *(buf + i))
		{
			stringitem += "\\r";
			len += 2;
			nInsert = 1;
		}
		else if ('\n' == *(buf + i))
		{
			stringitem += "\\n";
			len += 2;
			nInsert = 1;
		}
		else if ('\t' == *(buf + i))
		{
			stringitem += "\\t";
			len += 2;
			nInsert = 1;
		}
		else if (
			'\0' != *(buf + i + 1) && (i + 1) < WSTRLEN && 
			isSimGra2(*(unsigned short *)&buf[i],g_chrRet)
			)
		{
			stringitem += g_chrRet[0];
			++i;
			++len;
			stringitem += g_chrRet[1];
			++len;
			nInsert = 1;
		}
		else if (*(buf + i)>0x1B && *(buf + i)<0x7F)
		{
			stringitem += *(buf + i);
			++len;
			nInsert = 1;
		}
		else
		{
			nInsert = 0;
			break;
		}
		
		if ((i + 1) == WSTRLEN)
			break;
		++i;
	}
	
	
	if (nInsert && stringitem.length()>1)
	{
		if (i >= WSTRLEN)
		{
			stringitem.replace(WSTRLEN - 4, 4, " ...");
			len = WSTRLEN;
		}
		stringitem.replace(len, 1, "\0");
		
		strAsc=stringitem;
	}
	else
	{
		char ascTemp[]={"ASCII"};
		if (!memcmp((char*)(Dest-6),ascTemp,5))
			strcpy((char*)(Dest-6),"");
		else if (!memcmp((char*)(Dest-9),ascTemp,5))
			strcpy((char*)(Dest-9),"");


		return 0;
	}
	
	return RealStrToText((DWORD)strAsc.c_str(),(DWORD)strAsc.length(),Dest,MaxLen);
	
}
int StrToText(DWORD Str,DWORD StrLen,DWORD Dest,DWORD MaxLen)
{
	string strAsc;
	
    int             i=0;
    int             len = 0;
    int             nInsert=0;
	unsigned char g_chrRet[2];
    string          stringitem;
	stringitem.erase();
	
	unsigned char * buf=(unsigned char *)Str;
	
	while (('\0' != *(buf + i)) || ('\0' != *(buf + i + 1)) && (i + 1 < WSTRLEN))
	{
		if (IsAlpha2(*(unsigned short *)&buf[i],g_chrRet))
		{
			stringitem += g_chrRet[0];
			++i;
			++len;
			stringitem += g_chrRet[1];
			++len;
			nInsert = 1;
		}
		else if (IsAscII2(*(unsigned short *)&buf[i]))
		{
			stringitem += *(buf + i);
			++i;
			++len;
			nInsert = 1;
		}
		else if ('\r' == *(buf + i) && (*(buf + i + 1) == '\0'))
		{
			stringitem += "\\r";
			++i;
			len += 2;
			nInsert = 1;
		}
		else if ('\n' == *(buf + i) && (*(buf + i + 1) == '\0'))
		{
			stringitem += "\\n";
			++i;
			len += 2;
			nInsert = 1;
		}
		else if ('\t' == *(buf + i) && (*(buf + i + 1) == '\0'))
		{
			stringitem += "\\t";
			++i;
			len += 2;
			nInsert = 1;
		}
		else
		{
			nInsert = 0;
			break;
		}
		
		if ((i + 1) == WSTRLEN)
			break;
		++i;
	}
	
	if (nInsert)
	{
		if (i >= WSTRLEN)
		{
			stringitem.replace(WSTRLEN - 4, 4, " ...");
			len = WSTRLEN;
		}
		stringitem.replace(len, 1, "\0");
		
		strAsc=stringitem;
	}
	else
	{
		strcpy((char*)(Dest-8),"");
		return 0;
	}
	
	return RealStrToText((DWORD)strAsc.c_str(),(DWORD)strAsc.length(),Dest,MaxLen);
}
bool GetUnicode()
{
	BYTE lpbyte[5] = {0xE8, 0xFB, 0x03, 0x00, 0x00};
	DWORD dwCallRtl,lentemp;
	
	dwCallRtl = 0x0047E21B;
	if (*(BYTE*)dwCallRtl==0xFC)
	{
		WriteProcessMemory((HANDLE)-1, (PVOID)dwCallRtl, lpbyte+1, 1, &lentemp);
		dwCallRtl = 0x004D54D0;
		WriteProcessMemory((HANDLE)-1, (PVOID)dwCallRtl, lpbyte+2, 1, &lentemp);
	}
	else
		return false;
	
	
	dwCallRtl = 0x0047E162;
	
	if (*(BYTE*)dwCallRtl==0xE8)
	{
		lentemp=(DWORD)IsStrW-dwCallRtl-5;
		RtlCopyMemory(lpbyte+1, &lentemp, 4);
		WriteProcessMemory((HANDLE)-1, (PVOID)dwCallRtl, lpbyte, 5, &lentemp);
	}
	else
		return false;
	
	dwCallRtl = 0x0047E183;
	
	if (*(BYTE*)dwCallRtl==0xE8)
	{
		lentemp=(DWORD)IsStrW-dwCallRtl-5;
		RtlCopyMemory(lpbyte+1, &lentemp, 4);
		WriteProcessMemory((HANDLE)-1, (PVOID)dwCallRtl, lpbyte, 5, &lentemp);
	}
	else
		return false;
	
	
	dwCallRtl = 0x0047DDA3;
	
	if (*(BYTE*)dwCallRtl==0xE8)
	{
		lentemp=(DWORD)IsStrA-dwCallRtl-5;
		RtlCopyMemory(lpbyte+1, &lentemp, 4);
		WriteProcessMemory((HANDLE)-1, (PVOID)dwCallRtl, lpbyte, 5, &lentemp);
	}
	else
		return false;
	
	dwCallRtl = 0x0047DDD2;
	
	if (*(BYTE*)dwCallRtl==0xE8)
	{
		lentemp=(DWORD)IsStrA-dwCallRtl-5;
		RtlCopyMemory(lpbyte+1, &lentemp, 4);
		WriteProcessMemory((HANDLE)-1, (PVOID)dwCallRtl, lpbyte, 5, &lentemp);
	}
	else
		return false;
	
	
	dwCallRtl = 0x0047E21F;
	
	if (*(BYTE*)dwCallRtl==0xE8)
	{
		RealStrToText=(RealStrToTextType)(dwCallRtl + *(PDWORD)(dwCallRtl+1) + 5);
		lentemp=(DWORD)StrToText-dwCallRtl-5;
		RtlCopyMemory(lpbyte+1, &lentemp, 4);
		WriteProcessMemory((HANDLE)-1, (PVOID)dwCallRtl, lpbyte, 5, &lentemp);
	}
	else
		return false;
	
	dwCallRtl = 0x0047E01F;
	
	if (*(BYTE*)dwCallRtl==0xE8)
	{
		lentemp=(DWORD)StrToTextA-dwCallRtl-5;
		RtlCopyMemory(lpbyte+1, &lentemp, 4);
		WriteProcessMemory((HANDLE)-1, (PVOID)dwCallRtl, lpbyte, 5, &lentemp);
	}
	else
		return false;
	
	dwCallRtl = 0x0047E109;
	
	if (*(BYTE*)dwCallRtl==0xE8)
	{
		lentemp=(DWORD)StrToTextA-dwCallRtl-5;
		RtlCopyMemory(lpbyte+1, &lentemp, 4);
		WriteProcessMemory((HANDLE)-1, (PVOID)dwCallRtl, lpbyte, 5, &lentemp);
	}
	else
		return false;

	dwCallRtl = 0x0047DFCD;
	
	if (*(BYTE*)dwCallRtl==0xE8)
	{
		lentemp=(DWORD)StrToTextA-dwCallRtl-5;
		RtlCopyMemory(lpbyte+1, &lentemp, 4);
		WriteProcessMemory((HANDLE)-1, (PVOID)dwCallRtl, lpbyte, 5, &lentemp);
	}
	else
		return false;



}
extc int _export cdecl ODBG_Plugininit(
									   int ollydbgversion,
									   HWND hw,
									   ulong *features
									   )
{
    int nRetCode;
    int nRetResult = 0;
	
    if (ollydbgversion < PLUGIN_VERSION)
    {
        nRetResult = -1;
        goto Exit0;
    }
	
    g_hWndMain = hw;
	
    nRetCode = Createsorteddata(
        &(g_ustrreftbl.data),
        "ustrref",
        sizeof(t_ustrref),
        10,
        (SORTFUNC *)UStrRefSortFunc,
        NULL
		);
	
    nRetCode = Registerpluginclass(
        g_szustrrefclass,
        NULL,
        g_hInstance,
        UStrRefWndProc
		);
    if (nRetCode < 0)
    {
        Destroysorteddata(&(g_ustrreftbl.data));
        nRetResult = -1;
        goto Exit0;
    }
	
    // Report plugin in the log window.
    Addtolist(0, 0, "中文搜索引擎 v%d.%d.%d", VERSIONY, VERSIONM,VERSIOND);
    Addtolist(0, -1, "  Written by Luo Cong");
    Addtolist(0, -1, "  Compiled on " __DATE__ " " __TIME__);
	
    if (
        (0 != Plugingetvalue(VAL_RESTOREWINDOWPOS)) &&
        (0 != Pluginreadintfromini(g_hInstance, g_szIniKey, 0))
		)
    {
        CreateUStrRefWindow();
    }

	if (GetUnicode()==false)
	{
		MessageBox(0,"由于OD版本问题，实时UNICODE转化无法开启,你可以到 http://hi.baidu.com/chinahanwu 给我反馈错误","错误",MB_OK | MB_ICONINFORMATION);
	}
	
Exit0:
    return nRetResult;
}

extc int _export cdecl ODBG_Pluginclose(void)
{
    Pluginwriteinttoini(
        g_hInstance,
        g_szIniKey,
        NULL != g_ustrreftbl.hw
		);
    return 0;
}

extc void _export cdecl ODBG_Plugindestroy(void)
{
    Unregisterpluginclass(g_szustrrefclass);
    Destroysorteddata(&(g_ustrreftbl.data));
}

extc int _export cdecl ODBG_Pluginmenu(int origin, char data[4096], void *item)
{
    t_dump *pd;
	
    switch (origin)
    {
    case PM_MAIN:
        strcpy(data, "0 &1 搜索 ASCII|1 &2 搜索 UNICODE|2 &3 智能搜索|3 &4 帮助|4 &5 关于");
        return 1;
    case PM_DISASM:     // Popup menu in Disassembler
        pd = (t_dump *)item;
        if (NULL == pd || 0 == pd->size)    // Window empty, don't add
            return 0;
        // Add item "中文搜索引擎"
        // if some part of Disassembler is selected
        if (pd->sel1 > pd->sel0)
        {
            sprintf(
                data,
                "0 &中文搜索引擎{"
                "0 &1 搜索 ASCII|1 &2 搜索 UNICODE|2 &3 智能搜索|3 &4 帮助|4 &5 关于"
                "}"
				);
        }
        return 1;
    }
    return 0;
}

int IsPrintAble(const unsigned char ch)
{
    int nRet;
	
    if (ch >= 0x20 && ch < 0x7f)
        nRet = 1;
    else if (0xff == ch)
        nRet = 0;
    else if (0x80 == (ch & 0x80))
        nRet = 1;
    else
        nRet = 0;
	
    return nRet;
}


int IsSimplifiedCH(unsigned short &dch)
{
    int nRet;
    unsigned char highbyte;
    unsigned char lowbyte;
	
    highbyte = dch & 0x00ff;
    lowbyte  = dch / 0x0100;
	
	if (highbyte==0 || lowbyte==0)
		return 0;
	
    //    区名    码位范围   码位数  字符数 字符类型
    // 双字节2区 B0A1—F7FE   6768    6763    汉字
    if ((highbyte >= 0xb0 && highbyte <= 0xf7) && (lowbyte  >= 0xa1 && lowbyte  <= 0xfe))
        return 1;
	
	char GBt[3];
	char GBs[3];
	memset(GBt,0,3);
	memcpy(GBt,&dch,2);
	memset(GBs,0,3);
	if (LCMapString(0x0804,LCMAP_SIMPLIFIED_CHINESE, GBt, 2, GBs, 2))
	{
		highbyte=GBs[0];
		lowbyte=GBs[1];
		if (highbyte==0 || lowbyte==0)
			return 0;
		if ((highbyte >= 0xb0 && highbyte <= 0xf7) && (lowbyte  >= 0xa1 && lowbyte  <= 0xfe))
		{
			memcpy(&dch,GBs,2);
			return 1;
		}
	}
	
    return 0;
}

int IsGraphicCH(const unsigned short dch)
{
    int nRet;
    unsigned char highbyte;
    unsigned char lowbyte;
	
    highbyte = dch & 0x00ff;
    lowbyte  = dch / 0x0100;
	
    //    区名    码位范围   码位数  字符数 字符类型
    // 双字节1区 A1A1—A9FE    846     718  图形符号
    if (
        (highbyte >= 0xa1 && highbyte <= 0xa9) &&
        (lowbyte  >= 0xa1 && lowbyte  <= 0xfe)
		)
        nRet = 1;
    //    区名    码位范围   码位数  字符数 字符类型
    // 双字节5区 A840—A9A0    192     166  图形符号
    else if (
        (highbyte >= 0xa8 && highbyte <= 0xa9) &&
        (lowbyte  >= 0x40 && lowbyte  <= 0xa0)
		)
        nRet = 1;
    else
        nRet = 0;
	
    return nRet;
}
bool IsAlpha(const unsigned short dch)
{
	g_chrRet[0]=0;
	g_chrRet[1]=0;
	
    UnicodeToGB2312(g_chrRet,dch);
	
	if (!IsSimplifiedCH(*(unsigned short *)&g_chrRet) && !IsGraphicCH(*(unsigned short *)&g_chrRet))
		return false;
	
	if ((g_chrRet[0]==0 || g_chrRet[1]==0))
		return false;
	else
		return true;
}
bool isSimGra(const unsigned short dch)
{
	g_chrRet[0]=0;
	g_chrRet[1]=0;
	
	memcpy(g_chrRet,&dch,2);
	
	if (IsSimplifiedCH(*(unsigned short *)&g_chrRet) || IsGraphicCH(*(unsigned short *)&g_chrRet))
		return true;
	else
		return false;
}
int IsAscII(const unsigned short dch)
{
    int nRet;
    unsigned char highbyte;
    unsigned char lowbyte;
	
    highbyte = dch & 0x00ff;
    lowbyte  = dch / 0x0100;
	
    if (highbyte>0x1B && highbyte<0x7F && lowbyte==0)
        nRet = 1;
    else
        nRet = 0;
	
    return nRet;
}

unsigned long GetCurrentEIP(void)
{
    t_thread* t2;
	
    t2 = Findthread(Getcputhreadid());
    return t2->reg.ip;
}
void FindASCII()
{
    int             i;
    t_disasm        da;
    ulong           base;
    ulong           size;
    t_ustrref       mark;
    t_memory        *pmem;
    ulong           CurEIP;
    int             nInsert;
    int             len = 0;
    ulong           index = 0;
    string          stringitem;
    uchar           buf[TEXTLEN];
    int             dataindex = 0;
    uchar           cmd[MAXCMDSIZE];
    ulong           cmdsize = MAXCMDSIZE;
	bool ReadBool;
	unsigned long immconstTemp;
	
    CurEIP = GetCurrentEIP();
	
    Getdisassemblerrange(&base, &size);
    while (index < size)
    {
        Readcommand(base + index, (char *)cmd);
        cmdsize = Disasm(
            cmd,
            MAXCMDSIZE,
            base + index,
            NULL,
            &da,
            DISASM_CODE,
            0
			);
		
        if (CurEIP == (base + index))
        {
            g_strings.push_back("(Initial CPU selection)");
            mark.index    = dataindex;
            mark.size     = 1;
            mark.type     = 0;
            mark.addr     = CurEIP;
            mark.iscureip = 1;  // Is Current EIP
            Addsorteddata(&(g_ustrreftbl.data), &mark);
            g_CurEIP_Item_Index = dataindex;
            ++dataindex;
        }
        if (
            (0 != memicmp(da.result, "push", 4)) &&
            (0 != memicmp(da.result, "mov", 3)) &&
            (0 != memicmp(da.result, "lea", 3))
			)
        {
            index += cmdsize;
            continue;
        }
		
        nInsert = 0;
		if (da.immconst)
			immconstTemp = da.immconst;
		else if (da.adrconst)
			immconstTemp = da.adrconst;
		else
			immconstTemp = 0;
		pmem=Findmemory(immconstTemp);
        if ((NULL != pmem) && (1==1 || ('\0' != pmem->sect[0])))
        {
            Readmemory(buf, immconstTemp,TEXTLEN,MM_RESTORE | MM_SILENT);
			ReadBool=false;
			
StartASCII:
			
            i = 0;
            len = 0;
            nInsert = 0;
            stringitem.erase();
			
            while (
                ('\0' != *(buf + i)) &&
                (i < TEXTLEN)
				)
            {
                if ('\r' == *(buf + i))
                {
                    stringitem += "\\r";
                    len += 2;
                    nInsert = 1;
                }
                else if ('\n' == *(buf + i))
                {
                    stringitem += "\\n";
                    len += 2;
                    nInsert = 1;
                }
                else if ('\t' == *(buf + i))
                {
                    stringitem += "\\t";
                    len += 2;
                    nInsert = 1;
                }
                else if (
					'\0' != *(buf + i + 1) && (i + 1) < TEXTLEN && 
					isSimGra(*(unsigned short *)&buf[i])
					)
                {
                    stringitem += g_chrRet[0];
                    ++i;
                    ++len;
                    stringitem += g_chrRet[1];
                    ++len;
                    nInsert = 1;
                }
				else if (*(buf + i)>0x1B && *(buf + i)<0x7F)
				{
                    stringitem += *(buf + i);
                    ++len;
                    nInsert = 1;
				}
                else
                {
                    nInsert = 0;
                    break;
                }
				
				if ((i + 1) == TEXTLEN)
					break;
                ++i;
            }   // while
            if (nInsert)
            {
                if (i >= TEXTLEN)
                {
                    stringitem.replace(TEXTLEN - 4, 4, " ...");
                    len = TEXTLEN;  // cut len to TEXTLEN
                }
                stringitem.replace(len, 1, "\0");
                g_strings.push_back(stringitem);
				Insertname(base + index,NM_COMMENT,(char*)stringitem.c_str());
                mark.index    = dataindex;
                mark.size     = 1;
                mark.type     = 0;
                mark.addr     = base + index;
                mark.iscureip = 0;  // Not Current EIP
                Addsorteddata(&(g_ustrreftbl.data), &mark);
                ++dataindex;
            }
			
			if (ReadBool==false && *(buf + 0)>0 && *(buf + 1)>0 && *(buf + 2)>0 && *(buf + 3)==0)
			{
				immconstTemp=*(unsigned long *)&buf;
				pmem = Findmemory(immconstTemp);
				if ((NULL != pmem) && ('\0' != pmem->sect[0]))
				{
					Readmemory(buf, immconstTemp,TEXTLEN,MM_RESTORE | MM_SILENT);
					ReadBool=true;
					goto StartASCII;
				}
			}
			
        }
        index += cmdsize;
        Progress(index * 1000 / size, "找到字符串: %d 个", dataindex);
    }   // while
    Progress(0, "$");
    Infoline(
        "共找到字符串: %d 个  -  中文搜索引擎 (ASCII Mode)",
        dataindex
		);
}
void FindUNICODE()
{
    int             i;
    t_disasm        da;
    ulong           base;
    ulong           size;
    t_ustrref       mark;
    t_memory        *pmem;
    ulong           CurEIP;
    int             nInsert;
    int             len = 0;
    int             nIsAlpha;
    ulong           index = 0;
    string          stringitem;
    uchar           buf[TEXTLEN];
    int             dataindex = 0;
    uchar           cmd[MAXCMDSIZE];
    ulong           cmdsize = MAXCMDSIZE;
	bool ReadBool;
	unsigned long immconstTemp;
	
    CurEIP = GetCurrentEIP();
	
    Getdisassemblerrange(&base, &size);
    while (index < size)
    {
        Readcommand(base + index, (char *)cmd);
        cmdsize = Disasm(
            cmd,
            MAXCMDSIZE,
            base + index,
            NULL,
            &da,
            DISASM_CODE,
            0
			);
		
        if (CurEIP == (base + index))
        {
            g_strings.push_back("(Initial CPU selection)");
            mark.index    = dataindex;
            mark.size     = 1;
            mark.type     = 0;
            mark.addr     = CurEIP;
            mark.iscureip = 1;  // Is Current EIP
            Addsorteddata(&(g_ustrreftbl.data), &mark);
            g_CurEIP_Item_Index = dataindex;
            ++dataindex;
        }
		
        if (
            (0 != memicmp(da.result, "push", 4)) &&
            (0 != memicmp(da.result, "mov", 3)) &&
            (0 != memicmp(da.result, "lea", 3))
			)
        {
            index += cmdsize;
            continue;
        }
		
        nInsert = 0;
		if (da.immconst)
			immconstTemp = da.immconst;
		else if (da.adrconst)
			immconstTemp = da.adrconst;
		else
			immconstTemp = 0;
		pmem=Findmemory(immconstTemp);
        if ((NULL != pmem) && (1==1 || ('\0' != pmem->sect[0])))
        {
            Readmemory(buf, immconstTemp,TEXTLEN,MM_RESTORE | MM_SILENT);
			ReadBool=false;
StartUnicode:
			
            i = 0;
            len = 0;
            nInsert = 0;
			nIsAlpha=0;
            stringitem.erase();
			
            while (('\0' != *(buf + i)) || ('\0' != *(buf + i + 1)) && (i + 1 < TEXTLEN))
            {
                if (IsAlpha(*(unsigned short *)&buf[i]))
                {
                    stringitem += g_chrRet[0];
                    ++i;
                    ++len;
                    stringitem += g_chrRet[1];
                    ++len;
                    nInsert = 1;
					nIsAlpha=1;
                }
				else if (IsAscII(*(unsigned short *)&buf[i]))
				{
                    stringitem += *(buf + i);
                    ++i;
                    ++len;
                    nInsert = 1;
				}
				/*
                else if (IsGraphicCH(*(unsigned short *)&buf[i]) || IsSimplifiedCH(*(unsigned short *)&buf[i]))
                {
				stringitem += *(buf + i);
				++i;
				++len;
				stringitem += *(buf + i);
				++len;
				nInsert = 1;
                }
				*/
                else if ('\r' == *(buf + i) && (*(buf + i + 1) == '\0'))
                {
                    stringitem += "\\r";
                    ++i;
                    len += 2;
                    nInsert = 1;
                }
                else if ('\n' == *(buf + i) && (*(buf + i + 1) == '\0'))
                {
                    stringitem += "\\n";
                    ++i;
                    len += 2;
                    nInsert = 1;
                }
                else if ('\t' == *(buf + i) && (*(buf + i + 1) == '\0'))
                {
                    stringitem += "\\t";
                    ++i;
                    len += 2;
                    nInsert = 1;
                }
                else
                {
                    nInsert = 0;
                    break;
                }
				
				if ((i + 1) == TEXTLEN)
					break;
                ++i;
            }
            if (nInsert)
            {
                if (i >= TEXTLEN)
                {
                    stringitem.replace(TEXTLEN - 4, 4, " ...");
                    len = TEXTLEN;  // cut len to TEXTLEN
                }
                stringitem.replace(len, 1, "\0");
                g_strings.push_back(stringitem);
				Insertname(base + index,NM_COMMENT,(char*)stringitem.c_str());
                mark.index    = dataindex;
                mark.size     = 1;
                mark.type     = 0;
                mark.addr     = base + index;
                mark.iscureip = 0;  // Not Current EIP
                Addsorteddata(&(g_ustrreftbl.data), &mark);
                ++dataindex;
            }
			
			if (ReadBool==false && *(buf + 0)>0 && *(buf + 1)>0 && *(buf + 2)>0 && *(buf + 3)==0)
			{
				immconstTemp=*(unsigned long *)&buf;
				pmem = Findmemory(immconstTemp);
				if ((NULL != pmem) && ('\0' != pmem->sect[0]))
				{
					Readmemory(buf, immconstTemp,TEXTLEN,MM_RESTORE | MM_SILENT);
					ReadBool=true;
					goto StartUnicode;
				}
			}
        }
        index += cmdsize;
        Progress(index * 1000 / size, "找到字符串: %d 个", dataindex);
    }   // while
    Progress(0, "$");
    Infoline(
        "共找到字符串: %d 个  -  中文搜索引擎 (UNICODE Mode)",
        dataindex
		);
}
void FindAUTO()
{
	
    int             i,iTemp1,iTemp2,iTemp3,iTemp4;
    t_disasm        da;
    ulong           base;
    ulong           size;
    t_ustrref       mark;
    t_memory        *pmem;
    ulong           CurEIP;
    int             nInsert,nInsert1,nInsert2,nVail1,nVail2,nVail3,nVail4;
    int             nIsAlpha;
    int             len = 0,len1=0,len2=0,len3=0,len4=0;
    ulong           index = 0;
    string          stringitem,stringitemTemp1,stringitemTemp2,stringitemTemp3,stringitemTemp4;
    uchar           buf[TEXTLEN];
    int             dataindex = 0;
    uchar           cmd[MAXCMDSIZE];
    ulong           cmdsize = MAXCMDSIZE;
	bool ReadBool;
	unsigned long immconstTemp;
	int AbleNum=0;
	char CheckComment[TEXTLEN];
	int DISASMTYPE;
	
	DISASMTYPE=DISASM_CODE;
	
    CurEIP = GetCurrentEIP();
	
    Getdisassemblerrange(&base, &size);
    while (index < size)
    {
		
        if (CurEIP == (base + index))
        {
            g_strings.push_back("(Initial CPU selection)");
            mark.index    = dataindex;
            mark.size     = 1;
            mark.type     = 0;
            mark.addr     = CurEIP;
            mark.iscureip = 1;  // Is Current EIP
            Addsorteddata(&(g_ustrreftbl.data), &mark);
            g_CurEIP_Item_Index = dataindex;
            ++dataindex;
        }
		
		if (AbleNum>=ABLEMAX)
		{
			DISASMTYPE=DISASM_SIZE;
			stringitem.erase();
			if (Findname(base + index,NM_COMMENT,CheckComment)>0)
			{
				stringitem=CheckComment;
				g_strings.push_back(stringitem);
				mark.index    = dataindex;
				mark.size     = 1;
				mark.type     = 0;
				mark.addr     = base + index;
				mark.iscureip = 0;  // Not Current EIP
				Addsorteddata(&(g_ustrreftbl.data), &mark);
				++dataindex;
			}
			index ++;
			continue;
		}
		else
		{

        Readcommand(base + index, (char *)cmd);
        cmdsize = Disasm(
            cmd,
            MAXCMDSIZE,
            base + index,
            NULL,
            &da,
            DISASMTYPE,
            0
			);


			if (
				(0 != memicmp(da.result, "push", 4)) &&
				(0 != memicmp(da.result, "mov", 3)) &&
				(0 != memicmp(da.result, "lea", 3))
				)
			{
				index += cmdsize;
				continue;
			}
			
			nInsert=0;
			nInsert1=0;
			nInsert2=0;
			if (da.immconst)
				immconstTemp = da.immconst;
			else if (da.adrconst)
				immconstTemp = da.adrconst;
			else
				immconstTemp = 0;
			pmem=Findmemory(immconstTemp);
			
			if ((NULL != pmem) && (1==1 || ('\0' != pmem->sect[0])))
			{
				Readmemory(buf, immconstTemp,TEXTLEN,MM_RESTORE | MM_SILENT);
				ReadBool=false;
				nVail3=0;
				nVail4=0;
				
StartAUTO:
				
				nInsert1=0;
				nVail1=0;
				nInsert2=0;
				nVail2=0;
				
				i = 0;
				len = 0;
				nInsert = 0;
				stringitem.erase();
				
				while (
					('\0' != *(buf + i)) &&
					(i < TEXTLEN)
					)
				{
					if ('\r' == *(buf + i))
					{
						stringitem += "\\r";
						len += 2;
						nInsert = 1;
						nVail1++;
					}
					else if ('\n' == *(buf + i))
					{
						stringitem += "\\n";
						len += 2;
						nInsert = 1;
						nVail1++;
					}
					else if ('\t' == *(buf + i))
					{
						stringitem += "\\t";
						len += 2;
						nInsert = 1;
						nVail1++;
					}
					else if (
						'\0' != *(buf + i + 1) && (i + 1) < TEXTLEN && 
						isSimGra(*(unsigned short *)&buf[i])
						)
					{
						stringitem += g_chrRet[0];
						++i;
						++len;
						stringitem += g_chrRet[1];
						++len;
						nInsert = 1;
						nVail1++;
						
					}
					else if (*(buf + i)>0x1B && *(buf + i)<0x7F)
					{
						stringitem += *(buf + i);
						++len;
						nInsert = 1;
						nVail1++;
					}
					else
					{
						nInsert = 0;
						break;
					}
					if ((i + 1) == TEXTLEN)
						break;
					++i;
				}
				
				if (nInsert)
				{
					stringitemTemp1=stringitem;
					nInsert1=1;
					len1=len;
					iTemp1=i;
				}
				
				i = 0;
				len = 0;
				nInsert = 0;
				stringitem.erase();
				while (('\0' != *(buf + i)) || ('\0' != *(buf + i + 1)) && (i + 1 < TEXTLEN))
				{
					if (IsAlpha(*(unsigned short *)&buf[i]))
					{
						stringitem += g_chrRet[0];
						++i;
						++len;
						stringitem += g_chrRet[1];
						++len;
						nInsert = 1;
						nIsAlpha=1;
						nVail2++;
					}
					else if (IsAscII(*(unsigned short *)&buf[i]))
					{
						stringitem += *(buf + i);
						++i;
						++len;
						nInsert = 1;
						nVail2++;
					}
					/*
					else if (IsGraphicCH(*(unsigned short *)&buf[i]) || IsSimplifiedCH(*(unsigned short *)&buf[i]))
					{
					stringitem += *(buf + i);
					++i;
					++len;
					stringitem += *(buf + i);
					++len;
					nInsert = 1;
					nVail2++;
					}
					*/
					else if ('\r' == *(buf + i) && (*(buf + i + 1) == '\0'))
					{
						stringitem += "\\r";
						++i;
						len += 2;
						nInsert = 1;
						nVail2++;
					}
					else if ('\n' == *(buf + i) && (*(buf + i + 1) == '\0'))
					{
						stringitem += "\\n";
						++i;
						len += 2;
						nInsert = 1;
						nVail2++;
					}
					else if ('\t' == *(buf + i) && (*(buf + i + 1) == '\0'))
					{
						stringitem += "\\t";
						++i;
						len += 2;
						nInsert = 1;
						nVail2++;
					}
					else
					{
						nInsert = 0;
						break;
					}
					if ((i + 1) == TEXTLEN)
						break;
					++i;
				}
				
				if (nInsert)
				{
					stringitemTemp2=stringitem;
					nInsert2=1;
					len2=len;
					iTemp2=i;
				}
				
				nInsert=0;
				
				if (nInsert1 && nInsert2)
				{
					if (nVail1>nVail2)
					{
						stringitem=stringitemTemp1;
						len=len1;
						i=iTemp1;
					}
					else if (nVail1<nVail2)
					{
						stringitem=stringitemTemp2;
						len=len2;
						i=iTemp2;
					}
					else
					{
						stringitem=stringitemTemp2;
						len=len2;
						i=iTemp2;
					}
					nInsert=1;
				}
				else if (nInsert1)
				{
					stringitem=stringitemTemp1;
					len=len1;
					i=iTemp1;
					nInsert=1;
				}
				else if (nInsert2)
				{
					stringitem=stringitemTemp2;
					len=len2;
					i=iTemp2;
					nInsert=1;
				}
				
				if (nInsert)
				{
					if (ReadBool==false)
					{
						stringitemTemp3=stringitem;
						len3=len;
						iTemp3=i;
						nVail3=nVail1>nVail2?nVail1:nVail2;
					}
					else
					{
						stringitemTemp4=stringitem;
						len4=len;
						iTemp4=i;
						nVail4=nVail1>nVail2?nVail1:nVail2;
					}
				}
				
				if (ReadBool==false && *(buf + 0)>0 && *(buf + 1)>0 && *(buf + 2)>0 && *(buf + 3)==0)
				{
					immconstTemp=*(unsigned long *)&buf;
					pmem = Findmemory(immconstTemp);
					if ((NULL != pmem) && ('\0' != pmem->sect[0]))
					{
						Readmemory(buf, immconstTemp,TEXTLEN,MM_RESTORE | MM_SILENT);
						ReadBool=true;
						goto StartAUTO;
					}
				}
				
				if (nVail3>0 && nVail4>0)
				{
					if (nVail3>nVail4)
					{
						stringitem=stringitemTemp3;
						len=len3;
						i=iTemp3;
						nInsert=1;
					}
					else if (nVail3<nVail4)
					{
						stringitem=stringitemTemp4;
						len=len4;
						i=iTemp4;
						nInsert=1;
					}
					else
					{
						stringitem=stringitemTemp3;
						len=len3;
						i=iTemp3;
						nInsert=1;
						
						if (i >= TEXTLEN)
						{
							stringitem.replace(TEXTLEN - 4, 4, " ...");
							len = TEXTLEN;  // cut len to TEXTLEN
						}
						stringitem.replace(len, 1, "\0");
						g_strings.push_back(stringitem);
						if (dataindex<CHECKMAX)
						{
							if (Findname(base + index,NM_COMMENT,CheckComment)>0)
							{
								if (!strcmp((char*)stringitem.c_str(),CheckComment))
									AbleNum++;
							}
						}
						Insertname(base + index,NM_COMMENT,(char*)stringitem.c_str());
						mark.index    = dataindex;
						mark.size     = 1;
						mark.type     = 0;
						mark.addr     = base + index;
						mark.iscureip = 0;  // Not Current EIP
						Addsorteddata(&(g_ustrreftbl.data), &mark);
						++dataindex;
						
						stringitem=stringitemTemp4;
						len=len4;
						i=iTemp4;
						nInsert=1;
					}
				}
				else if (nVail3>0)
				{
					stringitem=stringitemTemp3;
					len=len3;
					i=iTemp3;
					nInsert=1;
				}
				else if (nVail4>0)
				{
					stringitem=stringitemTemp4;
					len=len4;
					i=iTemp4;
					nInsert=1;
				}
				
				if (nInsert)
				{
					if (i >= TEXTLEN)
					{
						stringitem.replace(TEXTLEN - 4, 4, " ...");
						len = TEXTLEN;  // cut len to TEXTLEN
					}
					stringitem.replace(len, 1, "\0");
					g_strings.push_back(stringitem);	
					if (dataindex<CHECKMAX)
					{
						if (Findname(base + index,NM_COMMENT,CheckComment)>0)
						{
							if (!strcmp((char*)stringitem.c_str(),CheckComment))
								AbleNum++;
						}
					}
					Insertname(base + index,NM_COMMENT,(char*)stringitem.c_str());
					mark.index    = dataindex;
					mark.size     = 1;
					mark.type     = 0;
					mark.addr     = base + index;
					mark.iscureip = 0;  // Not Current EIP
					Addsorteddata(&(g_ustrreftbl.data), &mark);
					++dataindex;
				}
				
		}
		}
		
        index += cmdsize;
        Progress(index * 1000 / size, "找到字符串: %d 个", dataindex);
    }   // while
    Progress(0, "$");
    Infoline(
        "共找到字符串: %d 个  -  中文搜索引擎 (AUTO Mode)",
        dataindex
		);
}
void ShowAboutInfo()
{
    char info[1024];
	
    sprintf(
        info,
        "中文搜索引擎 plugin v%d.%d.%d\r\n"
        "Compiled on " __DATE__ ", " __TIME__ "\r\n"
        "版本：罗聪修改版\r\n"
        "感谢罗聪大哥、MSDN上的wyingquan的支持\r\n"
        "本插件仅搜索ANSI、Unicode字符集\r\n\r\n"
        "错误反馈请到 http://hi.baidu.com/chinahanwu/\r\n"
        "或 吾爱破解论坛 http://www.52pojie.cn\r\n"
        "		  吾爱破解  夜风流\r\n"
		,
        VERSIONY, VERSIONM,VERSIOND
		);
	
    MessageBox(
        g_hWndMain,
        info,
        "中文搜索引擎 plugin",
        MB_OK | MB_ICONINFORMATION
		);
}

extc void _export cdecl ODBG_Pluginaction(int origin, int action, void *item)
{
    ulong base;
    ulong size;
	
    Getdisassemblerrange(&base, &size);
	
    switch (origin)
    {
    case PM_MAIN:
        switch (action)
        {
        case 0:     // Find ASCII
            if (g_ustrreftbl.data.n)
            {
                Deletesorteddatarange(
                    &(g_ustrreftbl.data),
                    0,
                    g_ustrreftbl.data.n
					);
            }
            if (base && size)
            {
                if (!g_strings.empty())
                    g_strings.clear();
                FindASCII();
            }
            CreateUStrRefWindow();
            if (base && size)
            {
                Selectandscroll(&g_ustrreftbl, g_CurEIP_Item_Index, 2);
            }
            break;
        case 1:     // Find UNICODE
            if (g_ustrreftbl.data.n)
            {
                Deletesorteddatarange(
                    &(g_ustrreftbl.data),
                    0,
                    g_ustrreftbl.data.n
					);
            }
            if (base && size)
            {
                if (!g_strings.empty())
                    g_strings.clear();
                FindUNICODE();
            }
            CreateUStrRefWindow();
            if (base && size)
            {
                Selectandscroll(&g_ustrreftbl, g_CurEIP_Item_Index, 2);
            }
            break;
        case 2:     // AUTO
            if (g_ustrreftbl.data.n)
            {
                Deletesorteddatarange(
                    &(g_ustrreftbl.data),
                    0,
                    g_ustrreftbl.data.n
					);
            }
            if (base && size)
            {
                if (!g_strings.empty())
                    g_strings.clear();
                FindAUTO();
            }
            CreateUStrRefWindow();
            if (base && size)
            {
                Selectandscroll(&g_ustrreftbl, g_CurEIP_Item_Index, 2);
            }
            break;
        case 3:

			MessageBox(0,"^_^","提示",MB_OK | MB_ICONINFORMATION);

            break;
        case 4:     // About
            ShowAboutInfo();
            break;
        default:
            break;
        }
        break;
		case PM_DISASM:
			switch (action)
			{
			case 0:     // Find ASCII
				if (g_ustrreftbl.data.n)
				{
					Deletesorteddatarange(
						&(g_ustrreftbl.data),
						0,
						g_ustrreftbl.data.n
						);
				}
				if (!g_strings.empty())
					g_strings.clear();
				FindASCII();
				CreateUStrRefWindow();
				Selectandscroll(&g_ustrreftbl, g_CurEIP_Item_Index, 2);
				break;
			case 1:     // Find UNICODE
				if (g_ustrreftbl.data.n)
				{
					Deletesorteddatarange(
						&(g_ustrreftbl.data),
						0,
						g_ustrreftbl.data.n
						);
				}
				if (!g_strings.empty())
					g_strings.clear();
				FindUNICODE();
				CreateUStrRefWindow();
				Selectandscroll(&g_ustrreftbl, g_CurEIP_Item_Index, 2);
				break;
			case 2:     // Find AUTO
				if (g_ustrreftbl.data.n)
				{
					Deletesorteddatarange(
						&(g_ustrreftbl.data),
						0,
						g_ustrreftbl.data.n
						);
				}
				if (!g_strings.empty())
					g_strings.clear();
				FindAUTO();
				CreateUStrRefWindow();
				Selectandscroll(&g_ustrreftbl, g_CurEIP_Item_Index, 2);
				break;
			case 3:

				MessageBox(0,"^_^","提示",MB_OK | MB_ICONINFORMATION);

				break;
			case 4:     // About
				ShowAboutInfo();
				break;
			default:
				break;
			}   // switch (action)
			break;  // case PM_DISASM
    }
}

extc void _export cdecl ODBG_Pluginreset(void)
{
    Deletesorteddatarange(&(g_ustrreftbl.data), 0, 0xFFFFFFFF);
}
extc int _export cdecl ODBG_Pluginshortcut(int origin,int ctrl,int alt,int shift,int key,void *item) 
{
	if (ctrl==1 && alt==0 && shift==0 && key==68) 
	{
		if (g_ustrreftbl.data.n)
		{
			Deletesorteddatarange(
				&(g_ustrreftbl.data),
				0,
				g_ustrreftbl.data.n
				);
		}
		if (!g_strings.empty())
			g_strings.clear();
		FindAUTO();
		CreateUStrRefWindow();
		Selectandscroll(&g_ustrreftbl, g_CurEIP_Item_Index, 2);
		return 1; 
	};
	return 0;
};
