// Common.cpp : 구현 파일입니다.
//
#include "stdafx.h"
#include "Common.h"
#include "Inspector.h"

#define DELETE_LOG_DAY	 180

// CCommon
CCommon g_objCommon;

IMPLEMENT_DYNAMIC(CCommon, CWnd)

CCommon::CCommon()
{
}

CCommon::~CCommon()
{
}

BEGIN_MESSAGE_MAP(CCommon, CWnd)
END_MESSAGE_MAP()

// CCommon 메시지 처리기입니다.

BOOL CCommon::Read_Config()
{
	CIniFileCS INI(gsCurrentDir + "\\Config.ini");
	if (!INI.Check_File()) { AfxMessageBox("Config.ini File Not Found!!!"); return FALSE; }

	gData.nHostPort = INI.Get_Integer("DATA", "HOST_PORT", 0);
	gData.sEquipId = INI.Get_String("DATA", "EQUIP_ID", "");
	gData.bHandlerLog = INI.Get_Bool("DATA", "HANDLER_LOG", FALSE);
	gData.bHostLog = INI.Get_Bool("DATA", "HOST_LOG", FALSE);
	gData.sErrFile = INI.Get_String("DATA", "ERROR_FILE", "");
	gData.bJahwa = INI.Get_Bool("DATA", "JAHWA", FALSE);

	return TRUE;
}

void CCommon::Delete_LogAll()
{
	Delete_LogFile(gsCurrentDir + "\\Handler");
	Delete_LogFile(gsCurrentDir + "\\Host");
	Delete_LogFile(gsCurrentDir + "\\MES");
}

void CCommon::Delete_LogFile(CString sPath)
{
	CString strFindPath, strFilePath, strFileName;
	strFindPath.Format("%s\\*.*", sPath);

	CFileFind Finder;
	BOOL bContinue = Finder.FindFile(strFindPath, NULL);

	CTime DelTime =  CTime::GetCurrentTime() - CTimeSpan(DELETE_LOG_DAY, 0, 0, 0);

	while (bContinue) {
		bContinue = Finder.FindNextFile();

		if (Finder.IsDots()) continue;
		if (Finder.IsDirectory()) continue;

		strFileName = Finder.GetFileName();
		strFilePath.Format("%s\\%s", sPath, strFileName);

		if (strFileName.GetLength() < 8) DeleteFile(strFilePath);	// 불필요한 파일 삭제

		int nYear = atoi(strFileName.Left(4));
		int nMonth = atoi(strFileName.Mid(4, 2));
		int nDay = atoi(strFileName.Mid(6, 2));

		if (nYear > 2000 && nYear < 3000 && nMonth > 0 && nMonth < 13 && nDay > 0 && nDay < 32) {
			CTime LogTime(nYear, nMonth, nDay, 0, 0, 0, 0);
			if (LogTime > DelTime) continue;
		}

		DeleteFile(strFilePath);
	}
}

void CCommon::DoEvents(int nSleep)
{
	MSG msg;
	if (PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	if (nSleep > 0) Sleep(nSleep);
}

///////////////////////////////////////////////////////////////////////////////

int CCommon::Find_Data(CString sBarCode)
{
	int nFind = 999;
	if (sBarCode.GetLength() < 10) return nFind;

	for(int i=gMar.nCycleNo-1; i>=0; i--) {
		if (sBarCode == gMar.sBar[i]) return i;
	}
	for(int i=49; i>=0; i--) {
		if (sBarCode == gMar.sBar[i]) return i;
	}
	return nFind;
}

void CCommon::Clean_Data()
{
	gMar.nCycleNo = 0;
	for(int i=0; i<50; i++) gMar.sBar[i] = "";
	for(int i=0; i<50; i++) gMar.sLot[i] = "";
	for(int i=0; i<50; i++) gMar.nCnt[i] = 0;
	for(int i=0; i<50; i++) for(int j =0; j<20; j++) gMar.sNGcd[i][j] = "";
	for(int i=0; i<50; i++) for(int j =0; j<20; j++) for(int k=0; k<5; k++) gMar.nData[i][j][k] = 0;
}


BOOL CCommon::LoadIniToVector(const CString& filePath, std::vector<CIniItem>& outVec, int &outCnt)
{
	CStdioFile file;
	
	int nDataCnt = 0;
	
	if (!file.Open(filePath, CFile::modeRead | CFile::typeText))
	{
		AfxMessageBox(_T("Failed to open ini file: ") + filePath);
		return FALSE;
	}


	outVec.clear();

	CString line;
	CString currentSection;

	while (file.ReadString(line))
	{
		line = Trim(line);

		if (line.IsEmpty())
			continue;

		// 주석 (; 또는 #)
		if (line[0] == ';' || line[0] == '#')
			continue;

		// 섹션 [SECTION]
		if (line.Left(1) == _T("[") && line.Right(1) == _T("]"))
		{
			currentSection = line.Mid(1, line.GetLength() - 2);
			currentSection = Trim(currentSection);
			continue;
		}

		// key=value 파싱
		int eqPos = line.Find(_T("="));
		if (eqPos < 0)
			continue;

		CString key   = Trim(line.Left(eqPos));
		CString value = Trim(line.Mid(eqPos + 1));

		CIniItem item;
		item.section = currentSection;
		item.key     = key;
		item.value   = value;

		outVec.push_back(item);
		nDataCnt++;
		gData.nRMSPgr++;
	}

	outCnt = nDataCnt;

	file.Close();
	return TRUE;

}

BOOL CCommon::SaveVectorToIni(const CString& filePath, const std::vector<CIniItem>& vec)
{
	CStdioFile file;
	if (!file.Open(filePath, CFile::modeWrite | CFile::modeCreate | CFile::typeText))
	{
		AfxMessageBox(_T("Failed to open ini file for write: ") + filePath);
		return FALSE;
	}

	CString lastSection;

	for (size_t i = 0; i < vec.size(); ++i)
	{
		const CIniItem& item = vec[i];

		// 섹션이 바뀌면 출력
		if (item.section != lastSection)
		{
			if (!item.section.IsEmpty())
			{
				CString secLine;
				secLine.Format(_T("[%s]\n"), item.section);
				file.WriteString(secLine);
			}
			lastSection = item.section;
		}

		CString line;
		line.Format(_T("%s=%s\n"), item.key, item.value);
		file.WriteString(line);
	}

	file.Close();
	return TRUE;
}