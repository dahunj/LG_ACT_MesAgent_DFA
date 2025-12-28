// Host.cpp : 구현 파일입니다.
//
#include "stdafx.h"
#include "MesAgent.h"
#include "Host.h"
#include "Inspector.h"
#include "Common.h"
#include "LogFile.h"
#include "MesAgentDlg.h"
#include "Handler.h"

IMPLEMENT_DYNAMIC(CHost, CWnd)

CHost g_objHost;

const char STX = 0x02;
const char ETX = 0x03;
const CString CRLF = "\r\n";

// CHost

CHost::CHost()
{
	m_bConnected = FALSE;
	m_bHostOnline = FALSE;
	m_strStFn = "";
	m_strRcmd = "";

	m_dwLastTime = GetTickCount();
}

CHost::~CHost()
{
}

BEGIN_MESSAGE_MAP(CHost, CWnd)
	ON_MESSAGE(UM_SERVER_ACCEPT, &CHost::OnServerAccept)
	ON_MESSAGE(UM_SERVER_REMOVE, &CHost::OnServerRemove)
	ON_MESSAGE(UM_SERVER_RECEIVE, &CHost::OnServerReceive)
END_MESSAGE_MAP()

// CHost 메시지 처리기입니다.

void CHost::Initialize()
{
	m_bConnected = FALSE;
	m_bHostOnline = FALSE;
	m_nClientIdx = 0;
	m_Server.Listen_Socket(gData.nHostPort, this);
	m_nSendCmdCount = 0;
	gMes.nAHostCount = 0;
}

void CHost::Terminate()
{
	m_bConnected = FALSE;
	m_bHostOnline = FALSE;
	m_Server.Close_Socket();
	if (g_objHandler.Is_Connected()) g_objHandler.Set_ControlState(2);	// 1:Online, 2:Offline
}

/////////////////////////////////////////////////////////////////////////////

LRESULT CHost::OnServerAccept(WPARAM wClientIdx, LPARAM lServerPort)
{
	int nClient = (int)wClientIdx;
	int nServerPort = (int)lServerPort;

	CString strIP = "", strLog;
	UINT nPort = 0;
	if (!m_Server.Get_ClientInfo(nClient, strIP, nPort)) return 0;
	m_nClientIdx = nClient;

	strLog.Format("Host Connected. IP(%s), Port, %d", strIP, nPort);
	g_objLogFile.Save_HostLog(strLog);

	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	pMainDlg->Set_HostConnect(TRUE, strIP, nPort);

	m_dwLastTime = GetTickCount();
	m_bConnected = TRUE;

	if (g_objHandler.Is_Connected()) Set_S6F11_ControlState(1);	//1:Online, 2:Offline

	Set_S1F1();

	return 0;
}

LRESULT CHost::OnServerRemove(WPARAM wClientIdx, LPARAM lServerPort)
{
	m_bConnected = FALSE;
	m_bHostOnline = FALSE;

	CString strLog = "Host Disconnected.";
	g_objLogFile.Save_HostLog(strLog);

	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	pMainDlg->Set_HostConnect(FALSE, "0.0.0.0", 0);

	if (g_objHandler.Is_Connected()) g_objHandler.Set_ControlState(2);	// 1:Online, 2:Offline

	return 0;
}

LRESULT CHost::OnServerReceive(WPARAM wClientIdx, LPARAM lServerPort)
{
	int nClient = (int)wClientIdx;
	int nServerPort = (int)lServerPort;

	CString strIP = "";
	UINT nPort = 0;
	if (!m_Server.Get_ClientInfo(nClient, strIP, nPort)) return 0;

	BYTE byRecv[1025] = { 0 };	// Buffer 1024 -> Last 0x00
	int nLen = m_Server.Read_Socket(nClient, byRecv);

	// Unicode Multibyte 공통 사용 /////////////////////////////////////////////
	char *pRecv = (char*)byRecv;
	CString strRecvSocket = CString(pRecv);
	m_strRecvCmd += strRecvSocket;

	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	CString strLog, strMsg;

	while (!m_strRecvCmd.IsEmpty()) {
		int nStart = m_strRecvCmd.Find(STX);
		int nEnd = m_strRecvCmd.Find(ETX);

		if (nEnd < 0) break;	// 버퍼에 들어오는 중...

		if (nStart < 0 || nStart > nEnd) 
		{
			strLog.Format("[OnServerReceive] <<Error>> - Start(%d), End(%d).\n%s", nStart, nEnd, m_strRecvCmd);
			g_objLogFile.Save_HostLog(strLog);
			m_strRecvCmd.Delete(0, nEnd + 1);	// 쓰레기값이 채워져 있어서...
			continue;
		}

		m_dwLastTime = GetTickCount();	// 시간 갱신

		CString strRecv = m_strRecvCmd.Mid(nStart + 1, nEnd - nStart - 1);
		m_strRecvCmd.Delete(0, nEnd + 1);

		// Host Log /////////////////////////////////////////////////////////////////
		strLog.Format("[<-] %s", strRecv);
		g_objLogFile.Save_HostLog(strLog);

		m_nRecvCmdCount = atoi(strRecv.Mid(8, 4));	// 4Byte

		if (strRecv.GetAt(12) == '0') 
		{		// Heart Beat
//			strMsg.Format("%s : [HeartBeat]", strLog.Left(18));
//			pMainDlg->Set_HostMsg(strMsg);
			Reply_HeartBeat();

		}
		else 
		{
			CString strXml = strRecv.Right(strRecv.GetLength() - 13);
			if (!Extract_Xml(strXml)) return 0;

			strMsg.Format("%s : %s,%s", strLog.Left(18), m_strStFn, m_strRcmd); 
			pMainDlg->Set_HostMsg(strMsg);

			if		(m_strStFn == "S1F2") Get_S1F2();	// Are You There Data ==> S1F1 응답
			else if (m_strStFn == "S1F3") Get_S1F3();	// Current Recipe Name Request
			else if (m_strStFn == "S2F3") Get_S2F3();	// Link Test Request
			else if (m_strStFn == "S2F31") Get_S2F31(); // Date and Time Set Request
			else if (m_strStFn == "S7F19") Get_S7F19();	// Recip List Request
			else if (m_strStFn == "S10F3") Get_S10F3();	// Terminal Display, Single
			else if (m_strStFn == "S7F25") Get_S7F25();	// Formatted Process Program Request
			else if (m_strStFn == "S2F49") {			// Remote Command
				if		(m_strRcmd == "TRAY_LOT_START")	 Get_S2F49_LotStart();
				else if (m_strRcmd == "MGZ_ID_CONFIRM")	 Get_S2F49_MGZConfirm();
				else if (m_strRcmd == "TRAY_ID_CONFIRM") Get_S2F49_CarrierConfirm();
				else if (m_strRcmd == "LOT_ID_FAIL")	 Get_S2F49_LotCancel();
				else if (m_strRcmd == "MGZ_ID_FAIL")	 Get_S2F49_MGZCancel();
				else if (m_strRcmd == "TRAY_ID_FAIL")	 Get_S2F49_CarrierCancel();
				else if (m_strRcmd == "PRODUCT_DATA")	 Get_S2F49_ProductData();
				else if (m_strRcmd == "PRODUCT_ID_FAIL") Get_S2F49_Module_Fail();
				else if (m_strRcmd == "LOT_MODULE_DATA_DETAIL") Get_S2F49_Module_Data();
				else if (m_strRcmd == "PP_SELECT")				Get_S2F49_PPSelect();
				else if (m_strRcmd == "PP_UPLOAD_CONFIRM")		Get_S2F49_PPUploadConfirm();
				else if (m_strRcmd == "PP_UPLOAD_FAIL")			Get_S2F49_PPUploadFail();
			}
		}
	}
	return 0;
}

BOOL CHost::Extract_Xml(CString sXmlData)
{
	int k=0;
	m_strStFn = m_strRcmd = "";	// 초기화

	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	if (!m_xml.LoadXml(sXmlData) ) {
		CString strLog, strMsg;

		strMsg.Format("[Extract_Xml] CXml Data Load Fail.");
		pMainDlg->Set_HostMsg(strMsg);

		strLog.Format("%s\n%s", strMsg, sXmlData);
		g_objLogFile.Save_HostLog(strLog);

		return FALSE;
	}

	CXmlNode node = m_xml.GetRoot();
	m_strStFn = node.GetAttribute("ID");

	if (m_strStFn == "S2F31") 
	{
		CXmlNode nodeTime = m_xml.GetRoot()->GetChild("ITEM")->GetChild("TIME");
		m_strSetTime = nodeTime.GetAttribute("VALUE", "");

	} 
	else if(m_strStFn == "S1F3") 
	{
		CXmlNodes nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("SVIDLIST")->GetChildren();//GetChild("CPLIST");
		m_nS1F4AckNo = nodes.GetCount();

	}
	else if(m_strStFn == "S10F3") 
	{
		CXmlNode nodeTime = m_xml.GetRoot()->GetChild("ITEM");
		m_sHostMsg = nodeTime.GetChild("TEXT")->GetAttribute("VALUE");

	} 
	else if (m_strStFn == "S2F49")
	{
		CXmlNode nodeE = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("RCMD");
		m_strRcmd = nodeE.GetAttribute("VALUE", "");

		if (m_strRcmd == "TRAY_LOT_START") {
			CXmlNodes nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
			int nCount = nodes.GetCount();

			for (int i = 0; i < nCount; i++) {
				CString strName = nodes[i]->GetChild("CPNAME")->GetAttribute("VALUE");
				CString strData = nodes[i]->GetChild("CPVAL")->GetAttribute("VALUE");

				if (strName == "LOTID")		gMes.sHostLotId = strData;
				if (strName == "PROCID")	gMes.sHostProcID = strData;
				if (strName == "MODEL")		gMes.sHostModel = strData;
				if (strName == "TRAYID")	gMes.sHostTrayID = strData;
				if (strName == "RECIPEID")	gMes.sHostRecipe = strData;
				if (strName == "TOTALQTY")	gMes.nHostCmCount = atoi(strData);
				if (strName == "VENDOR_TYPE")		gMes.sHostVendor = strData;
				if (strName == "MODEL_CONFIG_CODE")	gMes.sHostConfig = strData;
			}
			Set_AddInfor(gMes.sHostLotId, gMes.sHostProcID, gMes.sHostModel, gMes.sHostRecipe);

		}
		else if (m_strRcmd == "LOT_ID_FAIL") 
		{
			CXmlNodes nodesF = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
			int nCount = nodesF.GetCount();

			for (int i = 0; i < nCount; i++) {
				CString strName = nodesF[i]->GetChild("CPNAME")->GetAttribute("VALUE");
				CString strData = nodesF[i]->GetChild("CPVAL")->GetAttribute("VALUE");
				if (strData.GetLength() < 3) strData = nodesF[i]->GetChild("CPACKC")->GetAttribute("VALUE");

				if (strName == "LOTID")		gMes.sCancelLotId = strData;
				if (strName == "RECIPEID")	gMes.sCancelRecipe = strData;
			}

			nodeE = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("RESULT");
			gMes.sCancelCode = nodeE.GetChild("CODE")->GetAttribute("VALUE");
			gMes.sCancelText = nodeE.GetChild("TEXT")->GetAttribute("VALUE");

		}
		else if (m_strRcmd == "MGZ_ID_FAIL")
		{
			CXmlNodes nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
			int nCount = nodes.GetCount();

			for (int i = 0; i < nCount; i++) {
				CString strName = nodes[i]->GetChild("CPNAME")->GetAttribute("VALUE");
				CString strData = nodes[i]->GetChild("CPVAL")->GetAttribute("VALUE");
				if (strData.GetLength() < 3) strData = nodes[i]->GetChild("CPACKC")->GetAttribute("VALUE");

				if (strName == "MGZID")	gMes.sCancelMGZID = strData;
			}

			nodeE = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("RESULT");
			gMes.sCancelCode = nodeE.GetChild("CODE")->GetAttribute("VALUE");
			gMes.sCancelText = nodeE.GetChild("TEXT")->GetAttribute("VALUE");

		}
		else if (m_strRcmd == "TRAY_ID_FAIL")
		{
			CXmlNodes nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
			int nCount = nodes.GetCount();

			for (int i = 0; i < nCount; i++) {
				CString strName = nodes[i]->GetChild("CPNAME")->GetAttribute("VALUE");
				CString strData = nodes[i]->GetChild("CPVAL")->GetAttribute("VALUE");
				if (strData.GetLength() < 3) strData = nodes[i]->GetChild("CPACKC")->GetAttribute("VALUE");

				if (strName == "TRAYID")	gMes.sCancelTrayID = strData;
			}

			nodeE = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("RESULT");
			gMes.sCancelCode = nodeE.GetChild("CODE")->GetAttribute("VALUE");
			gMes.sCancelText = nodeE.GetChild("TEXT")->GetAttribute("VALUE");

		}
		else if (m_strRcmd == "PRODUCT_DATA")
		{
 			CXmlNodes nodesPD = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
 			int nCount = nodesPD.GetCount();
 
 			for (int i = 0; i < nCount; i++) {
 				CString strName = nodesPD[i]->GetChild("CPNAME")->GetAttribute("VALUE");
 				CString strData = nodesPD[i]->GetChild("CPVAL")->GetAttribute("VALUE");
 
 				if (strName == "LOTID")		gMes.sPDHostLotId = strData;
				if (strName == "PROCID")	gMes.sPDHostProcID = strData;
				if (strName == "MODEL")		gMes.sPDHostModel = strData;
 				if (strName == "MODULEID")	gMes.sPDHostCmId = strData;
 				if (strName == "RESULT")	gMes.sPDHostJudge = strData;
 				if (strName == "DETAIL")	gMes.sPDHostDetail = strData;
 			}

		}
		else if (m_strRcmd == "PRODUCT_ID_FAIL") 
		{
			nodeE = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("RESULT");
			gMes.sCancelModule = nodeE.GetChild("MODULEID")->GetAttribute("VALUE");
			gMes.sCancelCode = nodeE.GetChild("CODE")->GetAttribute("VALUE");
			gMes.sCancelText = nodeE.GetChild("TEXT")->GetAttribute("VALUE");

		} 
		else if (m_strRcmd == "MGZ_ID_CONFIRM")
		{
			CXmlNodes nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
			int nCount = nodes.GetCount();

			for (int i = 0; i < nCount; i++) {
				CString strName = nodes[i]->GetChild("CPNAME")->GetAttribute("VALUE");
				CString strData = nodes[i]->GetChild("CPVAL")->GetAttribute("VALUE");

				if (strName == "MGZID")		gMes.sHostMGZID = strData;
			}

		} else if (m_strRcmd == "TRAY_ID_CONFIRM") {
			CXmlNodes nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
			int nCount = nodes.GetCount();

			for (int i = 0; i < nCount; i++) {
				CString strName = nodes[i]->GetChild("CPNAME")->GetAttribute("VALUE");
				CString strData = nodes[i]->GetChild("CPVAL")->GetAttribute("VALUE");

				if (strName	== "TRAYID")	gMes.sHostTrayID = strData;
			}

		} else if (m_strRcmd == "LOT_MODULE_DATA_DETAIL") {
			Clear_ModuleData();
			CXmlNodes nodeM = m_xml.GetRoot()->GetChild("ITEM")->GetChild("MODULELIST")->GetChildren();
			gMes.nModuleCount = nodeM.GetCount();
			if (gMes.nModuleCount <  0) gMes.nModuleCount = 0;
			if (gMes.nModuleCount > 40) gMes.nModuleCount = 40;

			for (int i = 0; i < gMes.nModuleCount; i++) {
//				CXmlNodes nodeA = nodeM[i]->GetChild("MODULEINFOLIST")->GetChildren();
				CXmlNodes nodeA = nodeM[i]->GetChildren();
				int nCount = nodeA.GetCount();

				for (int j = 0; j < 12; j++) {
					CString strName = nodeA[j]->GetChild("NAME")->GetAttribute("VALUE");
					CString strData = nodeA[j]->GetChild("VAL")->GetAttribute("VALUE");
					strData.Replace(",", ".");

					k = 0;
					if (strName	== "LOTID")			k = 0;
					if (strName	== "MODULEID")		k = 1;
					if (strName	== "SITE")			k = 2;
					if (strName	== "EQPID")			k = 3;
					if (strName	== "EQPNAME")		k = 4;
					if (strName	== "TOOL_CAVITY")	k = 5;
					if (strName	== "PARA")			k = 6;
					if (strName	== "DATE")			k = 7;
					if (strName	== "ROS_JUDGE")		k = 8;
					if (strName	== "DFA_LOTID")		k = 9;
					if (strName	== "POCKETNO")		k = 10;
					if (strName	== "HAIM_FAILURE_CODE")	k = 11;	//DFA만 처리
					gMes.sModuleData[i][k] = strData;

				}
			}

		} else if (m_strRcmd == "PP_SELECT") {
 			CXmlNodes nodesPD = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
 			int nCount = nodesPD.GetCount();
 
 			for (int i = 0; i < nCount; i++) {
 				CString strName = nodesPD[i]->GetChild("CPNAME")->GetAttribute("VALUE");
 				CString strData = nodesPD[i]->GetChild("CPVAL")->GetAttribute("VALUE");
 
 				if (strName == "LOTID")		gMes.sHostLotId = strData;
				if (strName == "PROCID")	gMes.sHostProcID = strData;
				if (strName == "PRODUCTID")	gMes.sHostModel = strData;
 				if (strName == "RECIPEID")	gMes.sHostRecipe = strData;

 			}

		} else if (m_strRcmd == "PP_UPLOAD_CONFIRM") {
			nodeE = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("RESULT");
			gMes.sCancelCode = nodeE.GetChild("CODE")->GetAttribute("VALUE");
			gMes.sCancelText = nodeE.GetChild("TEXT")->GetAttribute("VALUE");

		} else if (m_strRcmd == "PP_UPLOAD_FAIL") {
			CXmlNodes nodes = m_xml.GetRoot()->GetChild("ITEM")->GetChild("RCMDCP")->GetChild("CPLIST")->GetChildren();
			int nCount = nodes.GetCount();

			for (int i = 0; i < nCount; i++) {
				CString strName = nodes[i]->GetChild("CPNAME")->GetAttribute("VALUE");
				CString strData = nodes[i]->GetChild("CPVAL")->GetAttribute("VALUE");

				if (strName	== "CODE")	gMes.sCancelCode = strData;
				if (strName	== "TEXT")	gMes.sCancelText = strData;
			}

		}
	}
	m_xml.Close();
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// Get Command

void CHost::Get_S1F2()
{
	// S1F1 에 대한 응답
}

void CHost::Get_S1F3()
{
	if (m_nS1F4AckNo == 1) g_objHandler.Set_RecipeListRequest(FALSE);
	if (m_nS1F4AckNo == 3) Set_S1F4_State();
}

void CHost::Get_S7F19()
{
	g_objHandler.Set_RecipeListRequest(TRUE);
}

void CHost::Get_S2F3()
{
	Set_S2F4();
}

void CHost::Get_S2F31()
{
	CString strLog;
	if (m_strSetTime.GetLength() < 14) {
		strLog.Format("[Get_S2F31] Time Value Error => Time [%s]", m_strSetTime);
		g_objLogFile.Save_HostLog(strLog);
		return;
	}

	// PC Time set처리
	// UAC disable: 컴푸터.속성.관리센터.사용자 계정 컨터롤 설정변경.알리지 않음.저장
	//ShellExecute(NULL, "open", "cmd.exe", "/c time hh:mm:ss.sss", NULL, SW_HIDE);

	SYSTEMTIME sysTime;
	GetLocalTime(&sysTime);

	//m_strSetTime = "20180601123040";	// 2018-06-01 12:30:40
	sysTime.wYear = atoi(m_strSetTime.Mid(0, 4));
	sysTime.wMonth = atoi(m_strSetTime.Mid(4, 2));
	sysTime.wDay = atoi(m_strSetTime.Mid(6, 2));
	sysTime.wHour = atoi(m_strSetTime.Mid(8, 2));
	sysTime.wMinute = atoi(m_strSetTime.Mid(10, 2));
	sysTime.wSecond = atoi(m_strSetTime.Mid(12, 2));

	// 사용프로잭트속성.구성속성.링커.매니페스트파일(asInvoker->highestAvailable)
//	BOOL bOk = SetLocalTime(&sysTime);

	Set_S2F32();
	g_objHandler.Set_TimeSync();

	strLog.Format("[Get_S2F31] Time Set => Time [%s]", m_strSetTime);
	g_objLogFile.Save_HostLog(strLog);
}

void CHost::Get_S10F3()
{
	g_objHandler.Set_HostMsg(m_sHostMsg);
	Set_S10F4();
}

void CHost::Get_S7F25()
{
	g_objHost.Set_S7F26();
}

//-------------------------------------------------------------------------------------------------
void CHost::Get_S2F49_LotStart()
{
	Set_S2F50_LotStart();
	g_objHandler.Set_LotStart();
}

void CHost::Get_S2F49_LotCancel()
{
	Set_S2F50_LotCancel();
	g_objHandler.Set_LotCancel();
}

void CHost::Get_S2F49_ProductData()
{
	Set_S2F50_ProcuctData();
	g_objHandler.Set_CmResult();
}

void CHost::Get_S2F49_Module_Fail()
{
	Set_S2F50_Module_Fail();
	g_objHandler.Set_ModuleFail();
}

void CHost::Get_S2F49_Module_Data()
{
	Set_S2F50_ModuleData();
	g_objHandler.Set_ModuleData();
}

void CHost::Get_S2F49_CarrierCancel()
{
	Set_S2F50_CarrierCancel();
	g_objHandler.Set_CarrierCancel();
}

void CHost::Get_S2F49_MGZCancel()
{
	Set_S2F50_MGZCancel();
	g_objHandler.Set_MGZCancel();
}

void CHost::Get_S2F49_MGZConfirm()
{
	Set_S2F50_MGZConfirm();
	g_objHandler.Set_MGZConfirm();
}

void CHost::Get_S2F49_CarrierConfirm()
{
	Set_S2F50_CarrierConfirm();
	g_objHandler.Set_CarrierConfirm();
}

void CHost::Get_S2F49_PPSelect()
{
	Set_S2F50_PPSelect();
	g_objHandler.Set_PPSelect();
}

void CHost::Get_S2F49_PPUploadConfirm()
{
	Set_S2F50_PPUnloadConfirm();
	Set_S6F11_PPUploadReport(gMes.sHostLotId);
}

void CHost::Get_S2F49_PPUploadFail()
{
	Set_S2F50_PPUnloadFail();
	g_objHandler.Set_PPUnloadFail();
}

///////////////////////////////////////////////////////////////////////////////
// Set Command

void CHost::Set_S1F1()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S1F1\" NAME=\"Are You There Request\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S1F1");
}

void CHost::Set_S1F4_State()
{
	CString strControl, strEqiup, strVersion;

	strControl = g_objHandler.Is_Connected() ? "1" : "2";
	strEqiup.Format("%d", gData.nCurEquipState );
	strVersion.Format("%s", MAIN_VERSION);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S1F4\" NAME=\"Selected Equipment Status Data\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <SVLIST COUNT = \"3\">" + CRLF;
	strSend += "      <SV NAME=\"ControlState\" VALUE=\"" + strControl + "\" />" + CRLF;
	strSend += "      <SV NAME=\"EquipmentState\" VALUE=\"" + strEqiup + "\" />" + CRLF;
	strSend += "      <SV NAME=\"SWVersion\" VALUE=\"" + strVersion + "\" />" + CRLF;
	strSend += "    </SVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S1F4");
}

void CHost::Set_S1F4()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S1F4\" NAME=\"Selected Equipment Status Data\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <SVLIST COUNT=\"1\">" + CRLF;
	strSend += "      <SV NAME=\"SV\" VALUE=\"" + gData.sCurrentRecipe + "\" />" + CRLF;
	strSend += "    </SVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S1F4");
}

void CHost::Set_S7F20()
{
	CString strCount;
	strCount.Format("%d", gData.nRcpCount);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S7F20\" NAME=\"Delete Process Program Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <PPIDLIST COUNT=\"" + strCount + "\" >" + CRLF;
	
	for (int i = 0; i < gData.nRcpCount; i++) {
	strSend += "      <PPID VALUE=\"" + gData.sRecipList[i] + "\" />" + CRLF;
	}
	
	strSend += "    </PPIDLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S7F20");
}

void CHost::Set_S7F26()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	CString strTotal;
	strTotal.Format("%d", gData.nTotalCnt);

	strSend += "<EIF VERSION=\"2.0\" ID=\"S7F26\" NAME=\"Formatted Process Program Data\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <PPID VALUE=\"" + gMes.sHostRecipe + "\" />" + CRLF;
	strSend += "    <MDLN VALUE=\"0\"/>" + CRLF;
	strSend += "    <SOFTREV VALUE=\"" + gData.sVersion + "\" />" + CRLF;
	strSend += "    <LOTID VALUE=\"" + gMes.sHostLotId + "\" />" + CRLF;
	strSend += "    <PROCID VALUE=\"" + gMes.sHostProcID + "\" />" + CRLF;
	strSend += "    <PRODID VALUE=\"" + gMes.sHostModel + "\" />" + CRLF;
	strSend += "    <PCLIST COUNT=\"" + strTotal +"\">" + CRLF;


	for (int i = 0; i < nHandlerDataIdCount; i++) 
	{
		strSend += "    <LIST>" + CRLF;
		strSend += "      <CCODE VALUE=\"" + vecHandlerData[i].first + "\" />" + CRLF;
		strSend += "      <PPARM VALUE=\"" + vecHandlerData[i].second + "\" />" + CRLF;
		strSend += "    </LIST>" + CRLF;
	}
	
	for(int j = 0; j < 5; j++)
	{
		for (int i = 0; i < glistFAIInfo[j].size(); i++) 
		{
			strSend += "    <LIST>" + CRLF;
			strSend += "      <CCODE VALUE=\"" + glistFAIInfo[j][i].key + "\" />" + CRLF;
			strSend += "      <PPARM VALUE=\"" + glistFAIInfo[j][i].value + "\" />" + CRLF;
			strSend += "    </LIST>" + CRLF;
		}
	}

	for(int j = 0; j < 5; j++)
	{
		for (int i = 0; i < glistLightInfo[j].size(); i++) 
		{
		strSend += "    <LIST>" + CRLF;
		strSend += "      <CCODE VALUE=\"" + glistLightInfo[j][i].key + "\" />" + CRLF;
		strSend += "      <PPARM VALUE=\"" + glistLightInfo[j][i].value + "\" />" + CRLF;
		strSend += "    </LIST>" + CRLF;
		}
	}

	//for(int j = 0; j < 5; j++)
	//{
	//	for (int i = 0; i < glistParamInfo[j].size(); i++) 
	//	{
	//		strSend += "    <LIST>" + CRLF;
	//		strSend += "      <CCODE VALUE=\"" + glistParamInfo[j][i].key + "\" />" + CRLF;
	//		strSend += "      <PPARM VALUE=\"" + glistParamInfo[j][i].value + "\" />" + CRLF;
	//		strSend += "    </LIST>" + CRLF;
	//	}
	//}	

	strSend += "    </PCLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S7F26");
}

void CHost::Set_S2F4()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F4\" NAME=\"Link Test Response\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F4");
}

void CHost::Set_S2F32()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F32\" NAME=\"Date and Time Set Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <ACKC NAME=\"ACKC\" VALUE=\"" + m_strSetTime +"\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F32");
}

void CHost::Set_S10F4()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S10F4\" NAME=\"Terminal Display,Single Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <ACKC NAME=\"ACKC\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S10F4");
}

void CHost::Set_S6F11_ControlState(int nState)
{
	CString strState;
	strState.Format("%d", nState);

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"10101\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"10101\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"4\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"CONTROLSTATE\" VALUE=\"" + strState + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TEXT\" VALUE=\"\"/>" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "10101");

	if (nState == 1) g_objHandler.Set_ControlState(1);	// 1:Online, 2:Offline
	m_bHostOnline = (nState == 1 ? TRUE : FALSE);
}

void CHost::Set_S6F11_EquipState(int nState, int nErrNo)
{
	CString	strState, strErrNo, strOldState;
	strState.Format("%d", nState);
	strErrNo.Format("%d", nErrNo);
	if (nState != 6 || nErrNo < 1) { strErrNo = gData.sAlarmTxt = ""; }

	gData.nPreEquipState = gData.nPreEquipState == 0 ? 1 : gData.nCurEquipState;
	gData.nCurEquipState = nState;

	strOldState.Format("%d", gData.nPreEquipState);
// 	strOldState = ((nState == 2 || nState == 6) ? "5" : "6");

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"10102\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"10102\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"7\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PREVEQPSTATE\" VALUE=\"" + strOldState + "\" />" + CRLF;
	strSend += "      <DV NAME=\"CUREQPSTATE\" VALUE=\"" + strState + "\" />" + CRLF;
	strSend += "      <DV NAME=\"ALARMID\" VALUE=\"" + strErrNo + "\" />" + CRLF;
	strSend += "      <DV NAME=\"ALARMCODE\" VALUE=\"" + strErrNo + "\" />" + CRLF;
	strSend += "      <DV NAME=\"ALARMTEXT\" VALUE=\"" + gData.sAlarmTxt + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "10102");
}

void CHost::Set_S5F1_Alarm(int nSet, int nErrNo)
{
	CString sAlCD, sErrNo;
	if (nSet == 0)	sAlCD = "1";	//Reset
	else			sAlCD = "129";	//Set
	sErrNo.Format("%04d", nErrNo);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S5F1\" NAME=\"Alarm Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <ALCD NAME=\"ALCD\" VALUE=\"" + sAlCD + "\" />" + CRLF;
	strSend += "    <ALID NAME=\"ALID\" VALUE=\"" + sErrNo + "\" />" + CRLF;
	strSend += "    <ALTX NAME=\"ALTX\" VALUE=\"" + gData.sAlarmTxt + "\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S5F1");
}

void CHost::Set_S6F11_LotStart(CString sLotId, CString sMGZId, CString sSlot, CString sTrayID, CString sRecipe)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime, sPortID;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	Get_LotInfor(sLotId);
	sPortID = "101";

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20302\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20302\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"11\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTID\" VALUE=\"" + sPortID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PROCID\" VALUE=\"" + gMes.sHostProcID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PRODID\" VALUE=\"" + gMes.sHostModel + "\" />" + CRLF;
	strSend += "      <DV NAME=\"MGZID\" VALUE=\"" + sMGZId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"SLOTID\" VALUE=\"" + sSlot + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TRAYID\" VALUE=\"" + sTrayID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RECIPEID\" VALUE=\"" + gMes.sHostRecipe + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATIONMODE\" VALUE=\"N\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20302");
}

void CHost::Set_S6F11_CmRequest(CString sLotId, CString sCmId)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	Get_LotInfor(sLotId);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20403\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20403\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"7\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PROCID\" VALUE=\"" + gMes.sHostProcID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PRODID\" VALUE=\"" + gMes.sHostModel + "\" />" + CRLF;
//	strSend += "      <DV NAME=\"PRODUCTID\" VALUE=\"" + sCmId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"MODULEID\" VALUE=\"" + sCmId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATIONMODE\" VALUE=\"N\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20403");
}

void CHost::Set_S6F11_CmEnd(CString sLotId, CString strCmId, CString strResult, CString strNgCode, CString sFmMZ, CString sFmTray, CString sCmNo, CString sToTray, CString sOut, CString sROSJudge, CString sMarginal)
{
	//1번 기준 우상 -> 우하로 Convert
	int nPocket[40] = {37, 38, 39, 40, 33, 34, 35, 36, 29, 30,
					   31, 32, 25, 26, 27, 28, 21, 22, 23, 24,
					   17, 18, 19, 20, 13, 14, 15, 16,  9, 10,
					   11, 12,  5,  6,  7,  8,  1,  2,  3,  4 };
	SYSTEMTIME time;
	GetLocalTime(&time);

	int nINo = atoi(sCmNo);	if (nINo < 0 || nINo > 40) nINo = 0;
	int nONo = atoi(sOut);	if (nONo < 0 || nONo > 40) nONo = 0;

	CString strTime, sPortID, sNGOut, sCvtOut, sCvtCmNo, sDefectData, sDData;
	sCvtCmNo.Format("%d", nPocket[nINo-1]);
	sCvtOut.Format("%d", nPocket[nONo-1]);

	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	if (strResult == "OK") { sPortID = "201"; sNGOut = "0"; }
	else				   { sPortID = "301"; sNGOut = sCvtOut; }
	Get_LotInfor(sLotId);

	sDefectData = "";
	if (sMarginal == "OK") {
		int x = g_objCommon.Find_Data(strCmId);
		if (x < 50) {
			for(int i=0; i<gMar.nCnt[x]; i++) {
				sDData.Format("%s,%d,%d,%d,%d,%d,", gMar.sNGcd[x][i], gMar.nData[x][i][0], gMar.nData[x][i][1], gMar.nData[x][i][2], gMar.nData[x][i][3], gMar.nData[x][i][4]);
				sDefectData = sDefectData + sDData;
			}
		}
	}

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20401\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20401\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"21\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTID\" VALUE=\"" + sPortID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PROCID\" VALUE=\"" + gMes.sHostProcID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PRODID\" VALUE=\"" + gMes.sHostModel + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TRAYID\" VALUE=\"" + sToTray + "\" />" + CRLF;
	strSend += "      <DV NAME=\"FROMMGZ\" VALUE=\"" + sFmMZ + "\" />" + CRLF;
	strSend += "      <DV NAME=\"FROMTRAY\" VALUE=\"" + sFmTray + "\" />" + CRLF;
	strSend += "      <DV NAME=\"FROMPOCKET\" VALUE=\"" + sCvtCmNo + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TOMGZ\" VALUE=\"\"/>" + CRLF;
	strSend += "      <DV NAME=\"TOTRAY\" VALUE=\"" + sToTray + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TOPOCKET\" VALUE=\"" + sCvtOut + "\" />" + CRLF;
	strSend += "      <DV NAME=\"MODULEID\" VALUE=\"" + strCmId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RESULT\" VALUE=\"" + strResult + "\" />" + CRLF;
	strSend += "      <DV NAME=\"NGCODE\" VALUE=\"" + strNgCode + "\" />" + CRLF;
	strSend += "      <DV NAME=\"NGPOCKETID\" VALUE=\"" + sNGOut + "\" />" + CRLF;
	strSend += "      <DV NAME=\"ROSJUDGE\" VALUE=\"" + sROSJudge + "\" />" + CRLF;
	strSend += "      <DV NAME=\"MARGINAL_FLAG\" VALUE=\"" + sMarginal + "\" />" + CRLF;
	strSend += "      <DV NAME=\"DEFECT_INFO\" VALUE=\"" + sDefectData + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORMODE\" VALUE=\"N\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20401");
}

void CHost::Set_S6F11_LotEnd(CString sLotId, CString sMZId, CString sTrayId, CString sRecipeId, int nHCount, int nOk, int nNg)
{
	CString strCount, strHCount, strOk, strNg, sPortID;
	strOk.Format("%d", nOk);
	strNg.Format("%d", nNg);
	strHCount.Format("%d", nHCount);
	strCount.Format("%d", nOk + nNg);
	sPortID = "201";

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	Get_LotInfor(sLotId);
	m_sLastProcID = gMes.sHostProcID;

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20303\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20303\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"25\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTID\" VALUE=\"" + sPortID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PROCID\" VALUE=\"" + gMes.sHostProcID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PRODID\" VALUE=\"" + gMes.sHostModel + "\" />" + CRLF;
	strSend += "      <DV NAME=\"MGZID\" VALUE=\"" + sMZId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TRAYID\" VALUE=\"" + sTrayId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RECIPEID\" VALUE=\"" + sRecipeId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TOTALQTY\" VALUE=\"" + strHCount + "\" />" + CRLF;
	strSend += "      <DV NAME=\"INQTY\" VALUE=\"" + strCount + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OUTQTY\" VALUE=\"" + strOk + "\" />" + CRLF;
	strSend += "      <DV NAME=\"NGQTY\" VALUE=\"" + strNg + "\" />" + CRLF;

	strSend += "      <DV NAME=\"DEEP_LEARNING_INQTY\" VALUE=\"" + gData.sGMESData[0] + "\" />" + CRLF;
	strSend += "      <DV NAME=\"DEEP_LEARNING_OUTQTY\" VALUE=\"" + gData.sGMESData[1] + "\" />" + CRLF;
	strSend += "      <DV NAME=\"DEEP_LEARNING_NGQTY\" VALUE=\"" + gData.sGMESData[2] + "\" />" + CRLF;
	strSend += "      <DV NAME=\"DEEP_LEARNING_TIME_OUTQTY\" VALUE=\"" + gData.sGMESData[3] + "\" />" + CRLF;
	strSend += "      <DV NAME=\"DEEP_LEARNING_IN_IMG_QTY\" VALUE=\"" + gData.sGMESData[4] + "\" />" + CRLF;
	strSend += "      <DV NAME=\"ROS_INQTY\" VALUE=\"" + gData.sGMESData[5] + "\" />" + CRLF;
	strSend += "      <DV NAME=\"ROS_OUTQTY\" VALUE=\"" + gData.sGMESData[6] + "\" />" + CRLF;
	strSend += "      <DV NAME=\"ROS_NGQTY\" VALUE=\"" + gData.sGMESData[7] + "\" />" + CRLF;
	strSend += "      <DV NAME=\"ROS_REPAIR_QTY\" VALUE=\"" + gData.sGMESData[8] + "\" />" + CRLF;
	strSend += "      <DV NAME=\"ROS_SKIP_REPAIR_QTY\" VALUE=\"" + gData.sGMESData[9] + "\" />" + CRLF;
	strSend += "      <DV NAME=\"MVI_INQTY\" VALUE=\"" + gData.sGMESData[10] + "\" />" + CRLF;

	strSend += "      <DV NAME=\"OPERATIONMODE\" VALUE=\"N\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20303");
	Set_DelInfor(sLotId);
}

void CHost::Set_S6F11_LotAbort(CString sLotId)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20104\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20104\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"3\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20104");
	Set_DelInfor(sLotId);
}

void CHost::Set_S6F11_IdleReportSet(BOOL bSet)
{
	CString strCEID;

	int nCEID = bSet ? 50102 : 50103;
	strCEID.Format("%d", nCEID);

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"" + strCEID + "\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"" + strCEID + "\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"3\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"REASONCODE\" VALUE=\"" + gIdle.sCode + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", strCEID);
}

void CHost::Set_S6F11_Terminal()
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"90101\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"90101\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"3\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TERMINALMSGACK\" VALUE=\"0\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "90101");
}


void CHost::Set_S6F11_MGZIDReport(CString sType, CString sMGZId)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime, sPortID;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	if (sType == "G") sPortID = "201";
	else			  sPortID = "101";

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20203\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20203\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"6\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTID\" VALUE=\"" + sPortID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"MGZID\" VALUE=\"" + sMGZId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RECIPEID\" VALUE=\"\"/>" + CRLF;
	strSend += "      <DV NAME=\"OPERATIONMODE\" VALUE=\"N\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20203");
}

void CHost::Set_S6F11_MGZIDRemove(CString sType, CString sMGZId, CString sRecipeId)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime, sPortID;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	if (sType == "G") sPortID = "201";
	else			  sPortID = "301";

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20202\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20202\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"8\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTID\" VALUE=\"" + sPortID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"MGZID\" VALUE=\"" + sMGZId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"\"/>" + CRLF;
	strSend += "      <DV NAME=\"PROCID\" VALUE=\"" + m_sLastProcID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RECIPEID\" VALUE=\"" + sRecipeId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATIONMODE\" VALUE=\"N\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20202");
}

void CHost::Set_S6F11_CarrierIDReport(CString sType, CString sMGZID, CString sCarrierID)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime, sPortID;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	if		(sType == "G") sPortID = "201";
	else if (sType == "N") sPortID = "301";
	else				   sPortID = "101";

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20301\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20301\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"7\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTID\" VALUE=\"" + sPortID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"MGZID\" VALUE=\"" + sMGZID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TRAYID\" VALUE=\"" + sCarrierID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RECIPEID\" VALUE=\"\"/>" + CRLF;
	strSend += "      <DV NAME=\"OPERATIONMODE\" VALUE=\"N\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20301");
}

void CHost::Set_S6F11_CarrierOutReport(CString sMGZID, CString sCarrierID, CString sSlotNo)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime, sPortID;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	sPortID = "101";

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20310\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20310\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"9\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTID\" VALUE=\"" + sPortID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"\"/>" + CRLF;
	strSend += "      <DV NAME=\"PROCID\" VALUE=\"\"/>" + CRLF;
	strSend += "      <DV NAME=\"PRODID\" VALUE=\"\"/>" + CRLF;
	strSend += "      <DV NAME=\"MGZID\" VALUE=\"" + sMGZID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"SLOTID\" VALUE=\"" + sSlotNo + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TRAYID\" VALUE=\"" + sCarrierID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20310");
}

void CHost::Set_S6F11_CarrierInReport(CString sType, CString sLotId, CString sMGZID, CString sCarrierID, CString sSlotNo)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime, sPortID;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
	Get_LotInfor(sLotId);
	if		(sType == "G") sPortID = "201";
	else if (sType == "N") sPortID = "301";
	else				   sPortID = "101";

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"20311\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"20311\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"9\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PORTID\" VALUE=\"" + sPortID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PROCID\" VALUE=\"" + gMes.sHostProcID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PRODID\" VALUE=\"" + gMes.sHostModel + "\" />" + CRLF;
	strSend += "      <DV NAME=\"MGZID\" VALUE=\"" + sMGZID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"SLOTID\" VALUE=\"" + sSlotNo + "\" />" + CRLF;
	strSend += "      <DV NAME=\"TRAYID\" VALUE=\"" + sCarrierID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "20310");
}

void CHost::Set_S6F11_PPSelectReport(CString sLotId)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"40102\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"40102\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"6\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + sLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RECIPEID\" VALUE=\"" + gMes.sHostRecipe + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PROCID\" VALUE=\"" + gMes.sHostProcID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PRODID\" VALUE=\"" + gMes.sHostModel + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "40102");
}

void CHost::Set_S6F11_PPUploadReport(CString sLotId)
{
	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"40103\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"40103\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"4\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + gMes.sHostLotId + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RECIPEID\" VALUE=\"" + gMes.sHostRecipe + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "40103");
}

// S2F49에 대한 응답으로 S2F50 송신
void CHost::Set_S2F50_LotStart()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"LOT_START\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "LOT_START");
}

void CHost::Set_S2F50_LotCancel()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"LOT_ID_FAIL\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "LOT_ID_FAIL");
}

void CHost::Set_S2F50_ProcuctData()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"PRODUCT_DATA\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "PRODUCT_DATA");
}

void CHost::Set_S2F50_Module_Fail()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"PRODUCT_ID_FAIL\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "PRODUCT_ID_FAIL");
}

void CHost::Set_S2F50_CarrierConfirm()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"TRAY_ID_CONFIRM\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "TRAY_ID_CONFIRM");
}

void CHost::Set_S2F50_CarrierCancel()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"TRAY_ID_FAIL\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "TRAY_ID_FAIL");
}

void CHost::Set_S2F50_MGZConfirm()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"MGZ_ID_CONFIRM\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "MGZ_ID_CONFIRM");
}

void CHost::Set_S2F50_MGZCancel()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"MGZ_ID_FAIL\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "MGZ_ID_FAIL");
}

void CHost::Set_S2F50_ModuleData()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"LOT_MODULE_DATA_DETAIL\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "LOT_MODULE_DATA_DETAIL");
}

void CHost::Set_S2F50_PPSelect()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"PP_SELECT\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "PP_SELECT");
}

void CHost::Set_S2F50_PPUnloadConfirm()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"PP_UPLOAD_CONFIRM\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "PP_UPLOAD_CONFIRM");
}

void CHost::Set_S2F50_PPUnloadFail()
{
	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S2F50\" NAME=\"Enhanced Remote Command Acknowledge\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <RCMDCP>" + CRLF;
	strSend += "      <RCMD NAME=\"RCMD\" VALUE=\"PP_UPLOAD_FAIL\" />" + CRLF;
	strSend += "    </RCMDCP>" + CRLF;
	strSend += "    <HCACK NAME=\"HCACK\" VALUE=\"0\" />" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, TRUE, "S2F50", "PP_UPLOAD_FAIL");
}

///////////////////////////////////////////////////////////////////////////////

void CHost::Reply_HeartBeat()
{
	CString strLog, strMsg, strSendSocket;

	strSendSocket.Format("%c%08d%04d0%c", STX, 0, m_nRecvCmdCount, ETX);

	char chSend[16] = { 0 };	// 마지막 0x00
	int nLength = strSendSocket.GetLength();
	memcpy(chSend, (LPSTR)(LPCSTR)strSendSocket, nLength);

	if (!m_Server.Write_Socket(m_nClientIdx, (BYTE*)chSend, nLength)) return;
}

void CHost::Send_Command(CString sSend, BOOL bReply, CString sStFn, CString sRcmd)
{
	CString strLog, strMsg, strSendSocket, strTemp;

	int nLen = sSend.GetLength();

	if (!bReply) m_nSendCmdCount < 9999 ? m_nSendCmdCount++ : m_nSendCmdCount = 1;
	int nCount = (bReply ? m_nRecvCmdCount : m_nSendCmdCount);

	strSendSocket.Format("%c%08d%04d1%s%c", STX, nLen, nCount, sSend, ETX);

	char chSend[2000] = { 0 };	// Max 2000
	int nLength = strSendSocket.GetLength();

	if (nLength > 2000) {
		int nSendCount = strSendSocket.GetLength() / 2000 + 1;
		for (int i = 0; i < nSendCount; i++) {
			strTemp = strSendSocket.Mid(i * 2000, 2000);
			memset(chSend, 0x00, sizeof(char) * 2000);
			memcpy(chSend, strTemp, strTemp.GetLength());
			int nLenTemp = strTemp.GetLength();
			if (!m_Server.Write_Socket(m_nClientIdx, (BYTE*)chSend, nLenTemp)) return;
		}

	} else {
		memcpy(chSend, (LPSTR)(LPCSTR)strSendSocket, nLength);
		if (!m_Server.Write_Socket(m_nClientIdx, (BYTE*)chSend, nLength)) return;
	}

	// Host Log //////////////////////////////////////////////////////////////////
	strLog.Format("[->] %08d%04d1%s", nLen, nCount, sSend);
	g_objLogFile.Save_HostLog(strLog);

	strMsg.Format("%s : %s,%s", strLog.Left(18), sStFn, sRcmd);
	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	pMainDlg->Set_HostMsg(strMsg);
}

///////////////////////////////////////////////////////////////////////////////

void CHost::Test_Send()
{
	gMar.nCycleNo = 0;
	
	for(int i=0; i<50; i++) gMar.sBar[i] = "1234567890123456789012345678901234567890";
	for(int i=0; i<50; i++) gMar.sLot[i] = "123456789012345678901234567890";
	for(int i=0; i<50; i++) gMar.nCnt[i] = 20;
	for(int i=0; i<50; i++) for(int j =0; j<20; j++) {
		gMar.sNGcd[i][j].Format("AAAAAAAAAABBBBBBBBBB%10d", j+1);
	}
	for(int i=0; i<50; i++) for(int j =0; j<20; j++) for(int k=0; k<5; k++) gMar.nData[i][j][k] = 1234567890;

	SYSTEMTIME time;
	GetLocalTime(&time);

	CString strTime, sData, sData1;
	strTime.Format("%04d%02d%02d%02d%02d%02d", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);

	sData = "";
	for(int j =0; j<20; j++) {
		sData1.Format("%s,%d,%d,%d,%d,%d,", gMar.sNGcd[49][j], gMar.nData[99][j][0], gMar.nData[99][j][1],
											gMar.nData[49][j][2], gMar.nData[99][j][3], gMar.nData[99][j][4]);
		sData = sData + sData1;
	}

	CString strSend = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;

	strSend += "<EIF VERSION=\"2.0\" ID=\"S6F11\" NAME=\"Event Report\">" + CRLF;
	strSend += "  <ELEMENT>" + CRLF;
	strSend += "    <EQPID VALUE=\"" + gData.sEquipId + "\" />" + CRLF;
	strSend += "  </ELEMENT>" + CRLF;
	strSend += "  <ITEM>" + CRLF;
	strSend += "    <CEID NAME=\"CEID\" VALUE=\"40102\" />" + CRLF;
	strSend += "    <RPTID NAME=\"RPTID\" VALUE=\"40102\" />" + CRLF;
	strSend += "    <DVLIST COUNT=\"6\">" + CRLF;
	strSend += "      <DV NAME=\"TIME\" VALUE=\"" + strTime + "\" />" + CRLF;
	strSend += "      <DV NAME=\"LOTID\" VALUE=\"" + gMar.sLot[49] + "\" />" + CRLF;
	strSend += "      <DV NAME=\"RECIPEID\" VALUE=\"" + gMar.sBar[49] + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PROCID\" VALUE=\"" + gMes.sHostProcID + "\" />" + CRLF;
	strSend += "      <DV NAME=\"PRODID\" VALUE=\"" + sData + "\" />" + CRLF;
	strSend += "      <DV NAME=\"OPERATORID\" VALUE=\"" + gData.sOperId + "\" />" + CRLF;
	strSend += "    </DVLIST>" + CRLF;
	strSend += "  </ITEM>" + CRLF;
	strSend += "</EIF>";

	Send_Command(strSend, FALSE, "S6F11", "40102");

// 	CString strXml = "<?xml version=\"1.0\" encoding=\"utf-16\"?>" + CRLF;
// 
// 	strXml += "<EIF VERSION=\"2.0\" ID=\"S2F49\" NAME=\"Enhanced Remote Command\">" + CRLF;
// 	strXml += "  <ELEMENT>" + CRLF;
// 	strXml += "    <EQPID VALUE=\"LM1CAV0110\" />" + CRLF;
// 	strXml += "  </ELEMENT>" + CRLF;
// 	strXml += "  <ITEM>" + CRLF;
// 	strXml += "    <RCMDCP>" + CRLF;
// 	strXml += "      <RCMD NAME=\"RCMD\" VALUE=\"START\" />" + CRLF;
// 	strXml += "      <CPLIST COUNT=\"6\">" + CRLF;
// 	strXml += "        <CP>" + CRLF;
// 	strXml += "          <CPNAME NAME=\"CPNAME\" VALUE=\"TIME\" />" + CRLF;
// 	strXml += "          <CPVAL NAME=\"CPVAL\" VALUE=\"20180410101158\" />" + CRLF;
// 	strXml += "        </CP>" + CRLF;
// 	strXml += "        <CP>" + CRLF;
// 	strXml += "          <CPNAME NAME=\"CPNAME\" VALUE=\"MODELID\" />" + CRLF;
// 	strXml += "          <CPVAL NAME=\"CPVAL\" VALUE=\"\" />" + CRLF;
// 	strXml += "        </CP>" + CRLF;
// 	strXml += "        <CP>" + CRLF;
// 	strXml += "          <CPNAME NAME=\"CPNAME\" VALUE=\"LOTID\" />" + CRLF;
// 	strXml += "          <CPVAL NAME=\"CPVAL\" VALUE=\"GPEZ612BJ4729A\" />" + CRLF;
// 	strXml += "        </CP>" + CRLF;
// 	strXml += "        <CP>" + CRLF;
// 	strXml += "          <CPNAME NAME=\"CPNAME\" VALUE=\"RECIPEID\" />" + CRLF;
// 	strXml += "          <CPVAL NAME=\"CPVAL\" VALUE=\"NA\" />" + CRLF;
// 	strXml += "        </CP>" + CRLF;
// 	strXml += "        <CP>" + CRLF;
// 	strXml += "          <CPNAME NAME=\"CPNAME\" VALUE=\"TOTALQTY\" />" + CRLF;
// 	strXml += "          <CPVAL NAME=\"CPVAL\" VALUE=\"1055\" />" + CRLF;
// 	strXml += "        </CP>" + CRLF;
// 	strXml += "        <CP>" + CRLF;
// 	strXml += "          <CPNAME NAME=\"CPNAME\" VALUE=\"OPERATORID\" />" + CRLF;
// 	strXml += "          <CPVAL NAME=\"CPVAL\" VALUE=\"74596\" />" + CRLF;
// 	strXml += "        </CP>" + CRLF;
// 	strXml += "      </CPLIST>" + CRLF;
// 	strXml += "      <RESULT>" + CRLF;
// 	strXml += "        <CODE NAME=\"CODE\" VALUE=\"\" />" + CRLF;
// 	strXml += "        <TEXT VALUE=\"\" />" + CRLF;
// 	strXml += "      </RESULT>" + CRLF;
// 	strXml += "    </RCMDCP>" + CRLF;
// 	strXml += "  </ITEM>" + CRLF;
// 	strXml += "</EIF>";
// 
// 	CString strHead, strSend;
// 
// 	int nLen = strXml.GetLength();
// 	int nCount = 30;
// 	int nType = 1;
// 	strHead.Format("%08d%04d%d", nLen, nCount, nType);
// 
// 	strSend.Format("%c%s%s%c", STX, strHead, strXml, ETX);
// 	Test_Receive(strSend);
// 	int aaa = 100;
}

int CHost::Test_Receive(CString strRecvSocket)
{
 	CString strLog, strMsg;
 	m_strRecvCmd += strRecvSocket;
 
 	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
 	while (!m_strRecvCmd.IsEmpty()) {
 		int nStart = m_strRecvCmd.Find(STX);
 		int nEnd = m_strRecvCmd.Find(ETX);
 
 		if (nEnd < 0) break;	// 버퍼에 들어오는 중...
 
 		if (nStart < 0 || nStart > nEnd) {
 			strMsg.Format("[OnServerReceive] - Start(%d), End(%d).", nStart, nEnd);
 			pMainDlg->Set_HostMsg(strMsg);
 
 			strLog.Format("%s\n%s", strMsg, m_strRecvCmd);
 			g_objLogFile.Save_HostLog(strLog);
 
 			m_strRecvCmd.Delete(0, nEnd + 1);	// 쓰레기값이 채워져 있어서...
 			continue;
 		}
 
// 		m_dwLastRecvTime = GetTickCount();	// 시간 갱신
 
 		CString strRecv = m_strRecvCmd.Mid(nStart + 1, nEnd - nStart - 1);
 		m_strRecvCmd.Delete(0, nEnd + 1);
 
 		// Host Log /////////////////////////////////////////////////////////////////
 		strLog.Format("[<-] %s", strRecv);
 		g_objLogFile.Save_HostLog(strLog);
 
 		pMainDlg->Set_HostMsg(strLog.Left(18));
 
 		m_nRecvCmdCount = atoi(strRecv.Mid(8, 4));	// 4Byte
 
		if (strRecv.GetAt(12) == '0') { Send_Command("", FALSE, 0); return 0; }	// Heart Beat

		CString strXml = strRecv.Right(strRecv.GetLength() - 13);
		if (!Extract_Xml(strXml)) return 0;

		strMsg.Format("%s : %s,%s", strLog.Left(18), m_strStFn, m_strRcmd); 
		pMainDlg->Set_HostMsg(strMsg);

		if		(m_strStFn == "S1F2") Get_S1F2();	// Are You There Data ==> S1F1 응답
		else if (m_strStFn == "S1F3") Get_S1F3();	// Current Recipe Name Request
		else if (m_strStFn == "S2F3") Get_S2F3();	// Link Test Request
		else if (m_strStFn == "S2F31") Get_S2F31(); // Date and Time Set Request
		else if (m_strStFn == "S7F19") Get_S7F19();	// Recip List Request
		else if (m_strStFn == "S10F3") Get_S10F3();	// Terminal Display, Single
		else if (m_strStFn == "S2F49") 
		{			// Remote Command
			if		(m_strRcmd == "TRAY_LOT_START")	 Get_S2F49_LotStart();
			else if (m_strRcmd == "MGZ_ID_CONFIRM")	 Get_S2F49_MGZConfirm();
			else if (m_strRcmd == "TRAY_ID_CONFIRM") Get_S2F49_CarrierConfirm();
			else if (m_strRcmd == "LOT_ID_FAIL")	 Get_S2F49_LotCancel();
			else if (m_strRcmd == "MGZ_ID_FAIL")	 Get_S2F49_MGZCancel();
			else if (m_strRcmd == "TRAY_ID_FAIL")	 Get_S2F49_CarrierCancel();
			else if (m_strRcmd == "PRODUCT_DATA")	 Get_S2F49_ProductData();
			else if (m_strRcmd == "PRODUCT_ID_FAIL") Get_S2F49_Module_Fail();
			else if (m_strRcmd == "LOT_MODULE_DATA_DETAIL") Get_S2F49_Module_Data();
			else if (m_strRcmd == "PP_SELECT")				Get_S2F49_PPSelect();
			else if (m_strRcmd == "PP_UPLOAD_CONFIRM")		Get_S2F49_PPUploadConfirm();
			else if (m_strRcmd == "PP_UPLOAD_FAIL")			Get_S2F49_PPUploadFail();
		}
	}
	return 0;
}

void CHost::Test_WriteLog()
{
// 	Get_S1F1();				// 2.
// 	Get_S2F3();				// 3.
// 	Get_S2F31();			// 4.
// 	Get_S2F49_Start();		// 5.
// 	Get_S2F49_Cancel();		// 6.
// 	Get_S2F49_CmResult();	// 7.
// 	Set_S1F1();				// 8.
// 	Set_S5F1_ErrorUpdate(1, "2111", "Test Error Message");	// 9.
// 	Set_S6F11_ControlState(1);								// 10.
// 	Set_S6F11_EquipState(2, 0);								// 11.
// 	Set_S6F11_LotReady("LOT_ID_SAMPLE");	// 12.
// 	Set_S6F11_LotStart("LOT_ID_SAMPLE", 1000);	// 13.
// 	Set_S6F11_LotAbort("LOT_ID_SAMPLE");	// 15.
// 	Set_S6F11_CmRequest("LOT_ID_SAMPLE", "CM_ID_SAMPLE");	// 16.
// 	Set_S6F11_CmEnd("LOT_ID_SAMPLE", "CM_ID_SAMPLE", "OK", "NG_CODE1", 3,"00", 1.0, 2.0, 3.0);	// 17.
// 	Set_S2F61_IdleRequst();	// 18.
// 	Set_S6F11_IdleReport();	// 19.
// 	Set_S9F13();	// 20.
}

void CHost::Set_AddInfor(CString sLotId, CString sProcID, CString sProdID, CString sRcpID)
{
	for(int i=0; i<50; i++) {
		if(sLotId = gMes.sAHostLotId[i]) {
			gMes.sAHostLotId[i] = sLotId;
			gMes.sAHostProcID[i] = sProcID;
			gMes.sAHostProdID[i] = sProdID;
			gMes.sAHostRcpID[i] = sRcpID;
			return;
		}
	}

	gMes.sAHostLotId[gMes.nAHostCount] = sLotId;
	gMes.sAHostProcID[gMes.nAHostCount] = sProcID;
	gMes.sAHostProdID[gMes.nAHostCount] = sProdID;
	gMes.sAHostRcpID[gMes.nAHostCount] = sRcpID;
	gMes.nAHostCount++;
	if (gMes.nAHostCount >= 50) gMes.nAHostCount = 0;
}

void CHost::Set_DelInfor(CString sLotId)
{
	for(int i=0; i<51; i++) {
		if(sLotId == gMes.sAHostLotId[i]) {
			gMes.sAHostLotId[i] = "";
			gMes.sAHostProcID[i] = "";
			gMes.sAHostProdID[i] = "";
			gMes.sAHostRcpID[i] = "";
			return;
		}
	}
}

void CHost::Get_LotInfor(CString sLotId)
{
	for(int i=0; i<50; i++) {
		if(sLotId == gMes.sAHostLotId[i]) {
			gMes.sHostProcID = gMes.sAHostProcID[i];
			gMes.sHostModel  = gMes.sAHostProdID[i];
			gMes.sHostRecipe = gMes.sAHostRcpID[i];
			return;
		}
	}
}

void CHost::Clear_ModuleData()
{
	gMes.nModuleCount = 0;
	for(int i=0; i<40; i++) {
		for(int j=0; j<12; j++) {
			gMes.sModuleData[i][j] = "";
		}
	}
}

void CHost::Test_Set()
{
	CString m_sLarData, strHead, strSend, sTemp;

	m_sLarData = "";
	for(int i=0; i<40; i++) {
      m_sLarData = m_sLarData + "<MODULEINFOLIST COUNT=\"12\">";
      m_sLarData = m_sLarData + "<MODULEINFO>";
      m_sLarData = m_sLarData + "<NAME NAME=\"NAME\" VALUE=\"LOTID\" />";
      m_sLarData = m_sLarData + "<VAL NAME=\"VAL\" VALUE=\"GPSAA1BDE1W0EFA\" />";
      m_sLarData = m_sLarData + "</MODULEINFO>";
      m_sLarData = m_sLarData + "<MODULEINFO>";
      m_sLarData = m_sLarData + "<NAME NAME=\"NAME\" VALUE=\"MODULEID\" />";
      m_sLarData = m_sLarData + "<VAL NAME=\"VAL\" VALUE=\"LG4E052005B800004B5+CCQA11418+1\" />";
      m_sLarData = m_sLarData + "</MODULEINFO>";
      m_sLarData = m_sLarData + "<MODULEINFO>";
      m_sLarData = m_sLarData + "<NAME NAME=\"NAME\" VALUE=\"SITE\" />";
      m_sLarData = m_sLarData + "<VAL NAME=\"VAL\" VALUE=\"AA1AR01\" />";
      m_sLarData = m_sLarData + "</MODULEINFO>";
      m_sLarData = m_sLarData + "<MODULEINFO>";
      m_sLarData = m_sLarData + "<NAME NAME=\"NAME\" VALUE=\"EQPID\" />";
      m_sLarData = m_sLarData + "<VAL NAME=\"VAL\" VALUE=\"ATC-02351\" />";
      m_sLarData = m_sLarData + "</MODULEINFO>";
      m_sLarData = m_sLarData + "<MODULEINFO>";
      m_sLarData = m_sLarData + "<NAME NAME=\"NAME\" VALUE=\"EQPNAME\" />";
      m_sLarData = m_sLarData + "<VAL NAME=\"VAL\" VALUE=\"C4_Flex Attach #01\" />";
      m_sLarData = m_sLarData + "</MODULEINFO>";
      m_sLarData = m_sLarData + "<MODULEINFO>";
      m_sLarData = m_sLarData + "<NAME NAME=\"NAME\" VALUE=\"TOOL_CAVITY\" />";
      m_sLarData = m_sLarData + "<VAL NAME=\"VAL\" VALUE=\"R1\" />";
      m_sLarData = m_sLarData + "</MODULEINFO>";
      m_sLarData = m_sLarData + "<MODULEINFO>";
      m_sLarData = m_sLarData + "<NAME NAME=\"NAME\" VALUE=\"PARA\" />";
      m_sLarData = m_sLarData + "<VAL NAME=\"VAL\" VALUE=\"2013,1001\" />";
      m_sLarData = m_sLarData + "</MODULEINFO>";
      m_sLarData = m_sLarData + "<MODULEINFO>";
      m_sLarData = m_sLarData + "<NAME NAME=\"NAME\" VALUE=\"DATE\" />";
      m_sLarData = m_sLarData + "<VAL NAME=\"VAL\" VALUE=\"2024013111502241\" />";
      m_sLarData = m_sLarData + "</MODULEINFO>";
      m_sLarData = m_sLarData + "<MODULEINFO>";
      m_sLarData = m_sLarData + "<NAME NAME=\"NAME\" VALUE=\"ROS_JUDGE\" />";
      m_sLarData = m_sLarData + "<VAL NAME=\"VAL\" VALUE=\"REPAIR1\" />";
      m_sLarData = m_sLarData + "</MODULEINFO>";
      m_sLarData = m_sLarData + "<MODULEINFO>";
      m_sLarData = m_sLarData + "<NAME NAME=\"NAME\" VALUE=\"DFA_LOTID\" />";
      m_sLarData = m_sLarData + "<VAL NAME=\"VAL\" VALUE=\"AAA1\" />";
      m_sLarData = m_sLarData + "</MODULEINFO>";
      m_sLarData = m_sLarData + "<MODULEINFO>";
      m_sLarData = m_sLarData + "<NAME NAME=\"NAME\" VALUE=\"POCKETNO\" />";
								sTemp.Format("<VAL NAME=\"VAL\" VALUE=\"%d\" />", i+1);
      m_sLarData = m_sLarData + sTemp;
      m_sLarData = m_sLarData + "</MODULEINFO>";
      m_sLarData = m_sLarData + "<MODULEINFO>";
      m_sLarData = m_sLarData + "<NAME NAME=\"NAME\" VALUE=\"HAIM_FAILURE_CODE\" />";
      m_sLarData = m_sLarData + "<VAL NAME=\"VAL\" VALUE=\"BB_B_AAAAA12345678901234567890\" />";
      m_sLarData = m_sLarData + "</MODULEINFO>";
      m_sLarData = m_sLarData + "</MODULEINFOLIST>";
	}

	m_sXMLData = "";
		m_sXMLData = m_sXMLData + "<EIF VERSION=\"2.0\" ID=\"S2F49\" NAME=\"Enhanced Remote Command\">";
		m_sXMLData = m_sXMLData + "<ELEMENT>";
	    m_sXMLData = m_sXMLData + "<EQPID VALUE=\"AVI-00413\" />";
		m_sXMLData = m_sXMLData + "</ELEMENT>";
		m_sXMLData = m_sXMLData + "<ITEM>";
	    m_sXMLData = m_sXMLData + "<RCMDCP>";
		m_sXMLData = m_sXMLData + "<RCMD NAME=\"RCMD\" VALUE=\"LOT_MODULE_DATA_DETAIL\" />";
		m_sXMLData = m_sXMLData + "</RCMDCP>";
		m_sXMLData = m_sXMLData + "<MODULELIST COUNT=\"40\">";
		m_sXMLData = m_sXMLData + m_sLarData;
        m_sXMLData = m_sXMLData + "</MODULELIST>";
		m_sXMLData = m_sXMLData + "</ITEM>";
		m_sXMLData = m_sXMLData + "</EIF>";

 	int nLen = m_sXMLData.GetLength();
 	int nCount = 30;
 	int nType = 1;
 	strHead.Format("%08d%04d%d", nLen, nCount, nType);

 	strSend.Format("%c%s%s%c", STX, strHead, m_sXMLData, ETX);
 	Test_Receive(strSend);
}
