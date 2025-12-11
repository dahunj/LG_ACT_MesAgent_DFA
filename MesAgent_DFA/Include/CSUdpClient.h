// UdpSocketCS.h : 헤더 파일
//
#pragma once

#include <afxsock.h>		// MFC socket extensions

#define UM_UDP_CLIENT_RECEIVE	WM_USER+9011

class CUdpRecvCS : public CAsyncSocket
{
public:
	CUdpRecvCS();
	virtual ~CUdpRecvCS();
	virtual void OnReceive(int nErrorCode);

private:
	UINT	m_nLocalPort;	// Local Port

	CWnd*		m_pParent;
	int			m_nReadLen;
	BYTE*		m_pRecvBuff;

public:
	UINT Get_LocalPort() { return m_nLocalPort; }

	BOOL Open_Socket(UINT nLocalPort, CWnd* pParent = NULL);
	void Close_Socket();

	int Read_Socket(BYTE* pBuffer);
};

class CUdpSendCS : public CAsyncSocket
{
public:
	CUdpSendCS();
	virtual ~CUdpSendCS();

private:
	CWnd*		m_pParent;

public:
	BOOL Open_Socket(CWnd* pParent = NULL);
	void Close_Socket();

	BOOL Write_Socket(CString sHostIp, UINT nHostPort, BYTE* pBuffer, int nLength);
};

///////////////////////////////////////////////////////////////////////////////
