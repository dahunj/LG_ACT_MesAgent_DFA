// Host.h : 헤더 파일
//
#pragma once

#include "./CXml/Xml.h"

using namespace JWXml;

// CHost

class CHost : public CWnd
{
	DECLARE_DYNAMIC(CHost)

public:
	CHost();
	virtual ~CHost();

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg LRESULT OnServerAccept(WPARAM wClientIdx, LPARAM lServerPort);
	afx_msg LRESULT OnServerRemove(WPARAM wClientIdx, LPARAM lServerPort);
	afx_msg LRESULT OnServerReceive(WPARAM wClientIdx, LPARAM lServerPort);

private:
	CServerSocketCS m_Server;
	int		m_nClientIdx;
	BOOL	m_bConnected;
	BOOL	m_bHostOnline;

	CXml	m_xml;

	CString m_strRecvCmd, m_sXMLData;

	int		m_nRecvCmdCount;	// 4Byte
	int		m_nSendCmdCount;

	CString m_strStFn;	// StreamFunction (S1F1, S2F3, S2F31, S2F49, S6F12)
	CString m_strRcmd;	// RCMD Command (START, CANCEL, DATA, PERMIT)

	DWORD	m_dwLastTime;	// 마지막 통신 시간
	CString m_strSetTime;	// Host 설정 시간

	int		m_nS1F4AckNo;	
	CString m_sHostMsg;
	CString m_sLastProcID;

private:
	BOOL Extract_Xml(CString sXmlData);

	void Get_S1F2();	// Are You There Data ==> S1F1 응답
	void Get_S1F3();	// 현재 Recipe ID 조회 요청
	void Get_S2F3();	// Link Test Request
	void Get_S2F31();	// Date and Time Set Request
	void Get_S7F19();	// Recipe ID List 요청 
	void Get_S10F3();	// Terminal Display, Single
	void Get_S7F25();	// Formatted Process Program Request

	void Get_S2F49_LotStart();
	void Get_S2F49_LotCancel();
	void Get_S2F49_CarrierCancel();
	void Get_S2F49_MGZCancel();
	void Get_S2F49_MGZConfirm();
	void Get_S2F49_CarrierConfirm();
	void Get_S2F49_ProductData();
	void Get_S2F49_Module_Fail();
	void Get_S2F49_Module_Data();
	void Get_S2F49_PPSelect();
	void Get_S2F49_PPUploadConfirm();
	void Get_S2F49_PPUploadFail();

	void Reply_HeartBeat();	// Heart Beat
	void Send_Command(CString sSend, BOOL bReply, CString sStFn, CString sRcmd="");	// XML

public:
	void Initialize();
	void Terminate();

	BOOL Is_Connected() { return m_bConnected; }
	BOOL Is_HostOnline() { return m_bHostOnline; }
	DWORD Get_LastTime() { return m_dwLastTime; }

	void Set_S1F1();	// Are You There Request
	void Set_S1F4();	// S1F3 에 대한 응답 (현재 Recipe ID 회신)
	void Set_S1F4_State();
	void Set_S2F4();	// Link Test Response => S2F3 응답
	void Set_S2F32();	// Date and Time Set Acknowledge => S1F31 응답
	void Set_S5F1_Alarm(int nSet, int nErrNo);
	void Set_S7F20();	// S7F19에 대한 응답 (Recipe List 회신)
	void Set_S10F4();	// Terminal Display => S10F3 응답
	void Set_S7F26();	// S7F25에 대한 응답 (Recipe Body 회신)

	// Enhanced Remote Command Acknowledge, S2F49에 대한 응답
	void Set_S2F50_LotStart();			// LotStart Ack
	void Set_S2F50_LotCancel();			// LOT_ID_FAIL Ack
	void Set_S2F50_ProcuctData();		// PRODUCT_DATA Ack
	void Set_S2F50_Module_Fail();		// Module_Fail Ack

	void Set_S6F11_ControlState(int nState);			// 1:Online, 2:Offline
	void Set_S6F11_EquipState(int nState, int nErrNo);	// 2:Idle, 5:Run, 6:Down
	void Set_S6F11_IdleReportSet(BOOL bSet);

	void Set_S6F11_LotStart(CString sLotId, CString sMGZId, CString sSlot, CString sTrayID, CString sRecipe);
	void Set_S6F11_LotEnd(CString sLotId, CString sMZId, CString sTrayId, CString sRecipeId, int nHCount, int nOk, int nNg);
	void Set_S6F11_LotAbort(CString sLotId);				
	void Set_S6F11_CmRequest(CString sLotId, CString sCmId);	// Module=CM 전공정 DATA 요청
	void Set_S6F11_CmEnd(CString sLotId, CString strCmId, CString strResult, CString strNgCode, CString sFmMZ, CString sFmTray, CString sCmNo, CString sToTray, CString sOut, CString sROSJudge, CString sMarginal);
	void Set_S6F11_Terminal();
	void Set_S6F11_MGZIDReport(CString sType, CString sMGZId);
	void Set_S6F11_MGZIDRemove(CString sType, CString sMGZId, CString sRecipeId);
	void Set_S6F11_CarrierIDReport(CString sType, CString sMGZID, CString sCarrierID);
	void Set_S6F11_CarrierOutReport(CString sMGZID, CString sCarrierID, CString sSlotNo);
	void Set_S6F11_CarrierInReport(CString sType, CString sLotId, CString sMGZID, CString sCarrierID, CString sSlotNo);
	void Set_S6F11_PPSelectReport(CString sLotId);
	void Set_S6F11_PPUploadReport(CString sLotId);

	void Set_S2F50_MGZCancel();			// MGZ_CANCEL
	void Set_S2F50_MGZConfirm();		// MGZ_CONFIRM
	void Set_S2F50_CarrierCancel();		// TRAY_CANCEL Ack
	void Set_S2F50_CarrierConfirm();	// TRAY_ID_CONFIRM Ack
	void Set_S2F50_ModuleData();		// LOT_MODULE_DATA_DETAIL Ack
	void Set_S2F50_PPSelect();			// PP_SELECT Ack
	void Set_S2F50_PPUnloadConfirm();	// PP_UPLOAD_CONFIRM Ack
	void Set_S2F50_PPUnloadFail();		// PP_UPLOAD_FAIL Ack

	void Test_Send();
	void Test_WriteLog();
	int  Test_Receive(CString strRecvSocket);
	void Set_AddInfor(CString sLotId, CString sProcID, CString sProdID, CString sRcpID);
	void Set_DelInfor(CString sLotId);
	void Get_LotInfor(CString sLotId);

	void Test_Set();
	void Clear_ModuleData();
};

extern CHost g_objHost;

///////////////////////////////////////////////////////////////////////////////
