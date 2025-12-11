// Common.h : 헤더 파일
//
#pragma once

// CCommon

static CString Trim(const CString& s)
{
	CString temp = s;
	temp.TrimLeft();
	temp.TrimRight();
	return temp;
}

class CCommon : public CWnd
{
	DECLARE_DYNAMIC(CCommon)

public:
	CCommon();
	virtual ~CCommon();

protected:
	DECLARE_MESSAGE_MAP()

public:
	BOOL Read_Config();
	void Delete_LogAll();
	void Delete_LogFile(CString sPath);
	void DoEvents(int nSleep = 0);
	int	 Find_Data(CString sBarCode);
	void Clean_Data();


	BOOL LoadIniToVector(const CString& filePath, std::vector<CIniItem>& outVec, int &outCnt);
	BOOL SaveVectorToIni(const CString& filePath, const std::vector<CIniItem>& vec);

};

extern CCommon g_objCommon;

///////////////////////////////////////////////////////////////////////////////
