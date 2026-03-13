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

	//Vision RMS 
	BOOL LoadIniToVector(const CString& filePath, std::vector<CIniItem>& outVec, int &outCnt);
	BOOL SaveVectorToIni(const CString& filePath, const std::vector<CIniItem>& vec);

	void Load_RMSData();

	//Handler RMS
	CString Get_IniValues(const CString& iniPath, const CString& section, const CString& key);
	bool Check_EquipData(const CString& equipIniPath, const CString& dataId, CString& outValue);
		
	std::map<CString, CString>& Get_MoveData_FromMap();
	bool Sort_MoveData_Keys(const CString& canonSection,const CString& suffix,CString& outIniKey);
	
	CString RemoveMoveDataPrefix(const CString& section);
	bool Sort_MoveData(const CString& moveIniPath,	const CString& bestSection, const CString& canonSection, const CString& suffix,	CString& outIniKey);
	bool Check_MoveData(const CString& moveIniPath, const CString& dataId, CString& outValue);
	bool Check_IniKeys(const CString& iniPath, const CString& section, const CString& key);
	bool Get_IniValues(const CString& equipIniPath, const CString& moveIniPath,vectorPair& outVec);

};

extern CCommon g_objCommon;

///////////////////////////////////////////////////////////////////////////////
