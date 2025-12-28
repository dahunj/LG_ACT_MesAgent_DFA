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
	CString ReadIniString(const CString& iniPath, const CString& section, const CString& key);
	bool TryFindEquipValue(const CString& equipIniPath, const CString& dataId, CString& outValue);
	
	
	std::map<CString, CString>& GetMoveKeyRuleMap();
	bool ResolveMoveKeyBySectionAndSuffix(const CString& canonSection,const CString& suffix,CString& outIniKey);
	
	CString StripNumberPrefix(const CString& section);
	bool ResolveMoveKeyFallbackByExistingKey(const CString& moveIniPath,
		const CString& bestSection,    // 실제 섹션명 (숫자 포함 가능)
		const CString& canonSection,   // StripNumberPrefix(bestSection)
		const CString& suffix,
		CString& outIniKey);
	bool TryFindMoveValue(const CString& moveIniPath, const CString& dataId, CString& outValue);
	bool IniKeyExists(const CString& iniPath, const CString& section, const CString& key);
	bool BuildDataIdValueVector(const CString& equipIniPath, const CString& moveIniPath,vectorPair& outVec);

};

extern CCommon g_objCommon;

///////////////////////////////////////////////////////////////////////////////
