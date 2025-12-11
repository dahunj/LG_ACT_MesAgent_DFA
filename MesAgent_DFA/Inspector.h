// Inspector.h : 헤더 파일
//
#pragma once

const int INSPECTOR_ALL		= 0;	// ALL PC
const int INSPECTOR_PC1		= 1;	// Bottom,(Align1,2)
const int INSPECTOR_PC2		= 2;	// Top1-1
const int INSPECTOR_PC3		= 3;	// Top2-1
const int INSPECTOR_PC4		= 4;	// Top1-2
const int INSPECTOR_PC5		= 5;	// Top2-2

// CInspector

class CInspector : public CWnd
{
	DECLARE_DYNAMIC(CInspector)

public:
	CInspector();
	virtual ~CInspector();

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg LRESULT OnUdpReceive(WPARAM wLocalPort, LPARAM lParam);

private:
	CUdpSocketCS	m_UdpVisionPC1, m_UdpVisionPC2, m_UdpVisionPC3, m_UdpVisionPC4, m_UdpVisionPC5;

	CString		m_strRecvCmd;
	BOOL	m_bConnectVisionPC1, m_bConnectVisionPC2, m_bConnectVisionPC3, m_bConnectVisionPC4, m_bConnectVisionPC5;
	
	void DoEvents(int nSleep = 0);

	void Get_ConnectRequest(int nInspector);
	void Get_ConnectReply(int nInspector);
	void Get_ConnectEnd(int nInspector);
	void Get_VisionData(int nInspector, CString sRcvData);

	void Send_Command(int nInspector, CString strSend);

public:
	void Initialize();
	void Terminate();

	void Set_ConnectRequest(int nInspector);
	void Set_ConnectReply(int nInspector);
	void Set_ConnectEnd(int nInspector);
};

extern CInspector g_objInspector;

///////////////////////////////////////////////////////////////////////////////
