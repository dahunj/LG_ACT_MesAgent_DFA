// stdafx.h : 자주 사용하지만 자주 변경되지는 않는
// 표준 시스템 포함 파일 및 프로젝트 관련 포함 파일이 
// 들어 있는 포함 파일입니다.
#pragma once

#ifndef _SECURE_ATL
#define _SECURE_ATL 1
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // 거의 사용되지 않는 내용은 Windows 헤더에서 제외합니다.
#endif

#include "targetver.h"

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // 일부 CString 생성자는 명시적으로 선언됩니다.

// MFC의 공통 부분과 무시 가능한 경고 메시지에 대한 숨기기를 해제합니다.
#define _AFX_ALL_WARNINGS

#include <afxwin.h>         // MFC 핵심 및 표준 구성 요소입니다.
#include <afxext.h>         // MFC 확장입니다.

#include <afxdisp.h>        // MFC 자동화 클래스입니다.

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>           // Internet Explorer 4 공용 컨트롤에 대한 MFC 지원입니다.
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>             // Windows 공용 컨트롤에 대한 MFC 지원입니다.
#endif // _AFX_NO_AFXCMN_SUPPORT

#include <afxcontrolbars.h>     // MFC의 리본 및 컨트롤 막대 지원

#include <afx.h>
#include <vector>
#include <iostream>

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

// Library 추가
#include "CSIniFile.h" 
#include "CSServerSocket.h"
#include "CSUdpSocket.h"
#include "CSUdpClient.h"
#ifdef _DEBUG
	#pragma comment(lib, "CSIniFileD.lib")
	#pragma comment(lib, "CSServerSocketD.lib")
	#pragma comment(lib, "CSUdpSocketD.lib")
	#pragma comment(lib, "CSUdpClientD.lib")
#else
	#pragma comment(lib, "CSIniFileR.lib")
	#pragma comment(lib, "CSServerSocketR.lib")
	#pragma comment(lib, "CSUdpSocketR.lib")
	#pragma comment(lib, "CSUdpClientR.lib")
#endif

#define MAIN_VERSION	"1.1.9d"		//DFA

extern CString gsCurrentDir;		// 현재 프로젝트 폴더


//RMS

struct CIniItem
{
	CString section; // 섹션 이름 (없으면 빈 문자열)
	CString key;
	CString value;
};


extern std::vector<CIniItem> glistFAIInfo;
extern std::vector<CIniItem> glistLightInfo;
extern std::vector<CIniItem> glistParamInfo;





typedef struct {
	int			nHostPort;
	CString		sEquipId;
	BOOL		bHandlerLog;
	BOOL		bHostLog;
	CString		sErrFile;
	BOOL		bJahwa;
	CString		sOperId;

	CString		sCurrentRecipe;
	int			nRcpCount;
	CString		sRecipList[100];

	int			nPreEquipState;		// 1:init, 2:idle, 3:Setup, 4:Ready, 5:Run, 6:Pause(Down)
	int			nCurEquipState;

	CString		sHandlerLotID[6];
	CString		sHandlerPortID[6];
	CString		sHandlerCMCount[6];

	CString		sBarcode[6][20][40];
	CString		sJudge[6][20][40];
	CString		sNgCode[6][20][40];

	int			nAlarmID;
	CString		sAlarmTxt;
	CString		sGMESData[11];
	CString		sVersion;
	CString		sBodyData[4][50];	// 0:T1M-PC2(1~32), 1:T1S-PC4(1~32), 2:T2M-PC3(1~44), 3:T2S-PC5(1~44)
} GLOVAL_DATA;

typedef struct {
	CString		sHostLotId;
	int			nHostCmCount;
	CString		sHostProcID;
	CString		sHostModel;
	CString		sHostRecipe;
	CString		sHostTrayID;
	CString		sHostMGZID;
	CString		sHostVendor;
	CString		sHostConfig;

	CString		sCancelLotId;
	CString		sCancelRecipe;
	CString		sCancelMGZID;
	CString		sCancelTrayID;
	CString		sCancelCode;
	CString		sCancelText;
	CString		sCancelModule;

	CString		sPDHostLotId;
	CString		sPDHostProcID;
	CString		sPDHostModel;
	CString		sPDHostCmId;
	CString		sPDHostJudge;
	CString		sPDHostDetail;

	int			nAHostCount;
	CString		sAHostLotId[100];
	CString		sAHostProcID[100];
	CString		sAHostProdID[100];
	CString		sAHostRcpID[100];

	int			nModuleCount;
	CString		sModuleData[40][12];	//0:LOTID,1:MODULEID,2:SITE,3:EQPID,4:EQPNAME,5:TOOL_CAVITY,6:PARA,7:DATE,8:ROS_JUDGE,9:DFA_LOTID,10:POCKETNO,11:HAIM_Data
} GLOVAL_MES;

typedef struct {
	int			nCount;
	CString		sStartTime;
	CString		sEndTime;
	CString		sCode;
	CString		sText;
} GLOVAL_IDLE;

typedef struct {
	int		nCycleNo;
	CString sBar[50];
	CString sLot[50];
	int		nCnt[50];
	CString sNGcd[50][20];
	int		nData[50][20][5];
} GLOVAL_MARGINAL;

extern  GLOVAL_DATA	gData;
extern  GLOVAL_MES	gMes;
extern  GLOVAL_IDLE	gIdle;
extern  GLOVAL_MARGINAL	gMar;