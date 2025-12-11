// Inspector.cpp : 구현 파일입니다.
//
#include "stdafx.h"
#include "MesAgent.h"
#include "MesAgentDlg.h"
#include "Inspector.h"
#include "LogFile.h"
#include "Common.h"

#define UDP_HANDLER_IP		"192.168.0.11"
#define UDP_PC1_HOST_IP		"192.168.0.21"
#define UDP_PC2_HOST_IP		"192.168.0.22"
#define UDP_PC3_HOST_IP		"192.168.0.23"
#define UDP_PC4_HOST_IP		"192.168.0.24"
#define UDP_PC5_HOST_IP		"192.168.0.25"

#define UDP_PC1_LPORT		8101
#define UDP_PC1_HPORT		8101
#define UDP_PC2_LPORT		8102
#define UDP_PC2_HPORT		8102
#define UDP_PC3_LPORT		8103
#define UDP_PC3_HPORT		8103
#define UDP_PC4_LPORT		8104
#define UDP_PC4_HPORT		8104
#define UDP_PC5_LPORT		8105
#define UDP_PC5_HPORT		8105

/*
#define UDP_HANDLER_IP		"127.0.0.1"
#define UDP_PC1_HOST_IP		"127.0.0.1"
#define UDP_PC2_HOST_IP		"127.0.0.1"
#define UDP_PC3_HOST_IP		"127.0.0.1"
#define UDP_PC4_HOST_IP		"127.0.0.1"
#define UDP_PC5_HOST_IP		"127.0.0.1"

#define UDP_PC1_LPORT		8101
#define UDP_PC1_HPORT		8111
#define UDP_PC2_LPORT		8102
#define UDP_PC2_HPORT		8112
#define UDP_PC3_LPORT		8103
#define UDP_PC3_HPORT		8113
#define UDP_PC4_LPORT		8104
#define UDP_PC4_HPORT		8114
#define UDP_PC5_LPORT		8105
#define UDP_PC5_HPORT		8115
*/
IMPLEMENT_DYNAMIC(CInspector, CWnd)

CInspector g_objInspector;

CCriticalSection g_csInspector;	// Send_Command 문제 해결하기 위함

// CInspector

CInspector::CInspector()
{
	m_strRecvCmd = "";
	m_bConnectVisionPC1 = m_bConnectVisionPC2 = m_bConnectVisionPC3 = m_bConnectVisionPC4 = m_bConnectVisionPC5 = FALSE;
}

CInspector::~CInspector()
{
}

BEGIN_MESSAGE_MAP(CInspector, CWnd)
	ON_WM_TIMER()
	ON_MESSAGE(UM_UDP_RECEIVE, OnUdpReceive)
END_MESSAGE_MAP()

// CInspector 메시지 처리기입니다.

void CInspector::Initialize()
{
	m_bConnectVisionPC1 = m_bConnectVisionPC2 = m_bConnectVisionPC3 = m_bConnectVisionPC4 = m_bConnectVisionPC5 = FALSE;
	BOOL bVisionPC1Opened = m_UdpVisionPC1.Open_Socket(UDP_PC1_LPORT, UDP_PC1_HPORT, UDP_PC1_HOST_IP, this);
	BOOL bVisionPC2Opened = m_UdpVisionPC2.Open_Socket(UDP_PC2_LPORT, UDP_PC2_HPORT, UDP_PC2_HOST_IP, this);
	BOOL bVisionPC3Opened = m_UdpVisionPC3.Open_Socket(UDP_PC3_LPORT, UDP_PC3_HPORT, UDP_PC3_HOST_IP, this);
	BOOL bVisionPC4Opened = m_UdpVisionPC4.Open_Socket(UDP_PC4_LPORT, UDP_PC4_HPORT, UDP_PC4_HOST_IP, this);
	BOOL bVisionPC5Opened = m_UdpVisionPC5.Open_Socket(UDP_PC5_LPORT, UDP_PC5_HPORT, UDP_PC5_HOST_IP, this);
	if (bVisionPC1Opened) Set_ConnectRequest(INSPECTOR_PC1);
	if (bVisionPC2Opened) Set_ConnectRequest(INSPECTOR_PC2);
	if (bVisionPC3Opened) Set_ConnectRequest(INSPECTOR_PC3);
	if (bVisionPC4Opened) Set_ConnectRequest(INSPECTOR_PC4);
	if (bVisionPC5Opened) Set_ConnectRequest(INSPECTOR_PC5);

	g_objLogFile.Save_InspectorLog("[M->V0] , Vision All Open Request.");
}

void CInspector::Terminate()
{
	Set_ConnectEnd(INSPECTOR_ALL);

	m_UdpVisionPC1.Close_Socket();
	m_UdpVisionPC2.Close_Socket();
	m_UdpVisionPC3.Close_Socket();
	m_UdpVisionPC4.Close_Socket();
	m_UdpVisionPC5.Close_Socket();

	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
	pMainDlg->Set_VisionConnect(FALSE);
}

///////////////////////////////////////////////////////////////////////////////
// UDP Socket Message

LRESULT CInspector::OnUdpReceive(WPARAM wLocalPort, LPARAM lParam)
{
	UINT nPort = (UINT)wLocalPort;
	int nInspector = 0, nLen = 0;
	BYTE byRecv[1024] = { 0 };
	CString strLog;

	if (nPort == UDP_PC1_HPORT) { nInspector = INSPECTOR_PC1; nLen = m_UdpVisionPC1.Read_Socket(byRecv); }
	if (nPort == UDP_PC2_HPORT) { nInspector = INSPECTOR_PC2; nLen = m_UdpVisionPC2.Read_Socket(byRecv); }
	if (nPort == UDP_PC3_HPORT) { nInspector = INSPECTOR_PC3; nLen = m_UdpVisionPC3.Read_Socket(byRecv); }
	if (nPort == UDP_PC4_HPORT) { nInspector = INSPECTOR_PC4; nLen = m_UdpVisionPC4.Read_Socket(byRecv); }
	if (nPort == UDP_PC5_HPORT) { nInspector = INSPECTOR_PC5; nLen = m_UdpVisionPC5.Read_Socket(byRecv); }

	if (nInspector == 0 || nLen < 1) {
		strLog.Format("[M<-V%d] , Local Port (%d) Mismatch or Receive Data Zero (%d)", nInspector, nPort, nLen);
		g_objLogFile.Save_InspectorLog(strLog);
		return 0;
	}

	CString strRecvSocket;
	strRecvSocket.Format("%s", byRecv);
	m_strRecvCmd += strRecvSocket;

	while (!m_strRecvCmd.IsEmpty()) {
		int nStart = m_strRecvCmd.Find("@");
		int nEnd = m_strRecvCmd.Find("\n");

		if (nEnd < 0) break;	// 버퍼에 들어오는 중...

		if (nStart < 0 || nStart > nEnd) {
			strLog.Format("[M<-V%d] , <<Error>> %s : Start(%d), End(%d)", nInspector, m_strRecvCmd, nStart, nEnd);
			g_objLogFile.Save_InspectorLog(strLog);
			m_strRecvCmd.Delete(0, nEnd + 1);	// 쓰레기값이 채워져 있어서...
			continue;
		}

		CString strRecv = m_strRecvCmd.Mid(nStart + 1, nEnd - nStart - 1);
		m_strRecvCmd.Delete(0, nEnd + 1);

		char chSep = ',';
		CString strCmd, strOp;

		AfxExtractSubString(strCmd, strRecv, 0, chSep);
		AfxExtractSubString(strOp, strRecv, 1, chSep);

		// Inspector Log ////////////////////////////////////////
		strLog.Format("[M<-V%d] , %s", nInspector, strRecv);
		g_objLogFile.Save_InspectorLog(strLog);
//		CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
//		pMainDlg->Set_HandlerMsg(strLog);
		/////////////////////////////////////////////////////////

		if (strCmd == "CONNECT") {
			if (strOp == "REQUEST")	Get_ConnectRequest(nInspector);
			else if (strOp == "REPLY") Get_ConnectReply(nInspector);
			else if (strOp == "END") Get_ConnectEnd(nInspector);

		} else if (strCmd == "VISION") {
			if (strOp == "DATA") Get_VisionData(nInspector, strRecv);

		}
	}

	return 1;
}

///////////////////////////////////////////////////////////////////////////////
// Get Command

void CInspector::Get_ConnectRequest(int nInspector)
{
	if (nInspector == INSPECTOR_PC1) m_bConnectVisionPC1 = TRUE;
	if (nInspector == INSPECTOR_PC2) m_bConnectVisionPC2 = TRUE;
	if (nInspector == INSPECTOR_PC3) m_bConnectVisionPC3 = TRUE;
	if (nInspector == INSPECTOR_PC4) m_bConnectVisionPC4 = TRUE;
	if (nInspector == INSPECTOR_PC5) m_bConnectVisionPC5 = TRUE;
	Set_ConnectReply(nInspector);
}

void CInspector::Get_ConnectReply(int nInspector)
{
	if (nInspector == INSPECTOR_PC1) m_bConnectVisionPC1 = TRUE;
	if (nInspector == INSPECTOR_PC2) m_bConnectVisionPC2 = TRUE;
	if (nInspector == INSPECTOR_PC3) m_bConnectVisionPC3 = TRUE;
	if (nInspector == INSPECTOR_PC4) m_bConnectVisionPC4 = TRUE;
	if (nInspector == INSPECTOR_PC5) m_bConnectVisionPC5 = TRUE;

	if (m_bConnectVisionPC1 && m_bConnectVisionPC2 && m_bConnectVisionPC3 && m_bConnectVisionPC4 && m_bConnectVisionPC5) {
		CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
		pMainDlg->Set_VisionConnect(TRUE);
	}
}

void CInspector::Get_ConnectEnd(int nInspector)
{
	if (nInspector == INSPECTOR_PC1) m_bConnectVisionPC1 = FALSE;
	if (nInspector == INSPECTOR_PC2) m_bConnectVisionPC2 = FALSE;
	if (nInspector == INSPECTOR_PC3) m_bConnectVisionPC3 = FALSE;
	if (nInspector == INSPECTOR_PC4) m_bConnectVisionPC4 = FALSE;
	if (nInspector == INSPECTOR_PC5) m_bConnectVisionPC5 = FALSE;
}

void CInspector::Get_VisionData(int nInspector, CString sRcvData)
{
	char chSep = ',';
	int nCount = 0;
	CString sLog, strArg[10];

	//    0    1     2      3       4       5            6                 7              8        9
	//@VISION,DATA,Lot_ID,Barcode,NG-Code,검사영역(1~7),불량좌표X(Pixel),불량좌표Y(Pixel),ROI크기X,ROI크기Y\n	검사영역(1:Btm, 2:Top1, 3:Top2, 4:S1, 5:S2, 6:S3, 7:S4)
	for (int i = 0; i < 10; i++) AfxExtractSubString(strArg[i], sRcvData, i, chSep);
	if (strArg[2].GetLength() < 1 || strArg[3].GetLength() < 10) {
		sLog.Format("[M->V%d] , Data Error =>, %s", nInspector, sRcvData);
		g_objLogFile.Save_InspectorLog(sLog);
		return;
	}

	int x, y;
	x = g_objCommon.Find_Data(strArg[3]);
	if (x > 49) {
		x = gMar.nCycleNo;
		gMar.nCycleNo++;
		if (gMar.nCycleNo > 50) { x = 0; gMar.nCycleNo = 1; }
		y = 0; gMar.nCnt[x] = 1;
	} else {
		if (gMar.nCnt[x] >= 20) {
			sLog.Format("[M->V%d] , Data Max Error =>, %s", nInspector, sRcvData);
			g_objLogFile.Save_InspectorLog(sLog);
			return;
		}
		y = gMar.nCnt[x]; gMar.nCnt[x]++;
	}
	if (x < 0 || x > 49) x = 0;
	if (y < 0 || y > 19) x = 0;

	gMar.sLot[x] = strArg[2];
	gMar.sBar[x] = strArg[3];
	gMar.sNGcd[x][y] = strArg[4];
	gMar.nData[x][y][0] = atoi(strArg[5]);
	gMar.nData[x][y][1] = atoi(strArg[6]);
	gMar.nData[x][y][2] = atoi(strArg[7]);
	gMar.nData[x][y][3] = atoi(strArg[8]);
	gMar.nData[x][y][4] = atoi(strArg[9]);

	sLog.Format("[M<-V%d] , OK ==>,%s,%d,%d,%d,%s,%s", nInspector, gMar.sBar[x], gMar.nCycleNo, x, y, gMar.sLot[x], gMar.sNGcd[x][y]);
	g_objLogFile.Save_InspectorLog(sLog);
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// Set Command

void CInspector::Set_ConnectRequest(int nInspector)
{
	CString	strSendCmd;
	strSendCmd.Format("CONNECT,REQUEST");
	Send_Command(nInspector, strSendCmd);
}

void CInspector::Set_ConnectReply(int nInspector)
{
	CString	strSendCmd;
	strSendCmd.Format("CONNECT,REPLY");
	Send_Command(nInspector, strSendCmd);
}

void CInspector::Set_ConnectEnd(int nInspector)
{
	CString	strSendCmd;
	strSendCmd.Format("CONNECT,END");
	Send_Command(nInspector, strSendCmd);

	if (nInspector == INSPECTOR_ALL || nInspector == INSPECTOR_PC1) m_bConnectVisionPC1 = FALSE;
	if (nInspector == INSPECTOR_ALL || nInspector == INSPECTOR_PC2) m_bConnectVisionPC2 = FALSE;
	if (nInspector == INSPECTOR_ALL || nInspector == INSPECTOR_PC3) m_bConnectVisionPC3 = FALSE;
	if (nInspector == INSPECTOR_ALL || nInspector == INSPECTOR_PC4) m_bConnectVisionPC4 = FALSE;
	if (nInspector == INSPECTOR_ALL || nInspector == INSPECTOR_PC5) m_bConnectVisionPC5 = FALSE;
}

///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// UDP Socket Send Message
void CInspector::Send_Command(int nInspector, CString strSend)
{
	// Inspector Log //////////////////////////////////////
	CString strLog;
	strLog.Format("[M->V%d] , %s", nInspector, strSend);
	g_objLogFile.Save_InspectorLog(strLog);

//	CMesAgentDlg *pMainDlg = (CMesAgentDlg*)AfxGetMainWnd();
//	pMainDlg->Set_HandlerMsg(strLog);
	///////////////////////////////////////////////////////

	g_csInspector.Lock();	// Critical Section

	CString strSendSocket;
	strSendSocket.Format("@%s\n", strSend);

	char chSend[1024] = { 0 };
	int nLength = strSendSocket.GetLength();
	memcpy(chSend, (LPSTR)(LPCSTR)strSendSocket, nLength);

	if (nInspector == INSPECTOR_ALL || nInspector == INSPECTOR_PC1) m_UdpVisionPC1.Write_Socket((BYTE*)chSend, nLength);
	if (nInspector == INSPECTOR_ALL || nInspector == INSPECTOR_PC2) m_UdpVisionPC2.Write_Socket((BYTE*)chSend, nLength);
	if (nInspector == INSPECTOR_ALL || nInspector == INSPECTOR_PC3) m_UdpVisionPC3.Write_Socket((BYTE*)chSend, nLength);
	if (nInspector == INSPECTOR_ALL || nInspector == INSPECTOR_PC4) m_UdpVisionPC4.Write_Socket((BYTE*)chSend, nLength);
	if (nInspector == INSPECTOR_ALL || nInspector == INSPECTOR_PC5) m_UdpVisionPC5.Write_Socket((BYTE*)chSend, nLength);
	g_csInspector.Unlock();	// Critical Section
}
/////////////////////////////////////////////////////////////////////////////

void CInspector::DoEvents(int nSleep)
{
	MSG msg;
	if (PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	if (nSleep > 0) Sleep(nSleep);
}

/////////////////////////////////////////////////////////////////////////////
