// Common.cpp : 구현 파일입니다.
//
#include "stdafx.h"
#include "Common.h"
#include "Inspector.h"
#include "Handler.h"
#include "LogFile.h"

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
	gData.sRMSPath = INI.Get_String("DATA","RMS_PATH","");

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
		if(value == "TRUE")
		{
				value = "T";
		}
		if(value == "FALSE")
		{
				value = "F";
		}
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



void CCommon::Load_RMSData()
{
	CString strLog, strTemp;

	//항목 개수 체크 

	int nFAIReadCnt[5], nLightReadCnt[5], nParamReadCnt[5];

	memset(nFAIReadCnt, 0, sizeof(int)*5);
	memset(nLightReadCnt, 0, sizeof(int)*5);
	memset(nParamReadCnt, 0, sizeof(int)*5);

	memset(gData.bRMSLoad_FAI, 0, sizeof(BOOL)*5);
	memset(gData.bRMSLoad_Light, 0, sizeof(BOOL)*5);
	memset(gData.bRMSLoad_Param, 0, sizeof(BOOL)*5);


	//RMS_Validation.ini 파일에서 파일 개수들 불러와서 비교하기 

	for(int i = 0; i < 5; i++)
	{
		strTemp.Format("%d", i+1);

		if(g_objCommon.LoadIniToVector("D:\\Vision Data\\Recipe\\FaiMeasureSpec_DFA_PC"+strTemp+".ini", glistFAIInfo[i], nFAIReadCnt[i]))
		{
			if(nFAIReadCnt[i]-1 != gData.nFAICnt[i])
			{
				strLog.Format("[LoadIniToVector] <Cnt Mismatch> - nFAIReadCnt(%d), gData.nFAICnt(%d)\n", nFAIReadCnt[i], gData.nFAICnt[i]);
				AfxMessageBox(strLog);
				g_objLogFile.Save_HandlerLog(strLog);
				gData.bRMSLoad_FAI[i] = FALSE;
			}
			else
			{
				gData.bRMSLoad_FAI[i] = TRUE;
				strLog.Format("[LoadIniToVector] <Read Success> - nFAIReadCnt(%d), gData.nFAICnt(%d)\n", nFAIReadCnt[i], gData.nFAICnt[i]);
				g_objLogFile.Save_HandlerLog(strLog);
			}

		}
	}

	/*for(int i = 0; i < 5; i++)
	{
		strTemp.Format("%d", i+1);
		if(g_objCommon.LoadIniToVector("D:\\Vision Data\\Recipe\\InspectLightInfo_PC"+strTemp+".ini", glistLightInfo[i], nLightReadCnt[i]))
		{
			if(nLightReadCnt[i]-1 != gData.nLightCnt[i])
			{
				strLog.Format("[LoadIniToVector] <Cnt Mismatch> - nLightReadCnt(%d), gData.nLightCnt(%d)\n", nLightReadCnt[i], gData.nLightCnt[i]);
				AfxMessageBox(strLog);
				g_objLogFile.Save_HandlerLog(strLog);
				gData.bRMSLoad_Light[i] = FALSE;
			}
			else
			{
				gData.bRMSLoad_Light[i] = TRUE;
				strLog.Format("[LoadIniToVector] <Read Success> - nLightReadCnt(%d), gData.nLightCnt(%d)\n", nLightReadCnt[i], gData.nLightCnt[i]);
				g_objLogFile.Save_HandlerLog(strLog);
			}

		}
	}

	for(int i = 0; i < 5; i++)
	{
		strTemp.Format("%d", i+1);
		if(g_objCommon.LoadIniToVector("D:\\Vision Data\\Recipe\\InspectParam_PC"+strTemp+".ini", glistParamInfo[i], nParamReadCnt[i]))
		{
			if(nParamReadCnt[i]-1 != gData.nParamCnt[i])
			{
				strLog.Format("[LoadIniToVector] <Cnt Mismatch> - nParamReadCnt(%d), gData.nParamCnt(%d)\n", nParamReadCnt[i], gData.nParamCnt[i]);
				AfxMessageBox(strLog);
				g_objLogFile.Save_HandlerLog(strLog);
				gData.bRMSLoad_Param[i] = FALSE;
			}
			else
			{
				gData.bRMSLoad_Param[i] = TRUE;
				strLog.Format("[LoadIniToVector] <Read Success> - nParamReadCnt(%d), gData.nParamCnt(%d)\n", nParamReadCnt[i], gData.nParamCnt[i]);
				g_objLogFile.Save_HandlerLog(strLog);
			}			
		}
	}
*/

	int nCheck = 0;
	gData.bRMSLoad_ALL = FALSE;
	for(int i = 0; i < 5; i++)
	{
		if(gData.bRMSLoad_FAI[i]) nCheck++;
		if(gData.bRMSLoad_Light[i]) nCheck++;
		if(gData.bRMSLoad_Param[i]) nCheck++;
	}

	if(nCheck == FILE_COUNT)
	{
		gData.bRMSLoad_ALL = TRUE;
		g_objHandler.Set_RMSLoadDone();
		//AfxMessageBox("RMS Data Load Success");
		
	}
}




CString CCommon::ReadIniString(const CString& iniPath, const CString& section, const CString& key)
{
	TCHAR buf[512] = {0};
	::GetPrivateProfileString(section, key, _T(""), buf, 512, iniPath);
	return CString(buf);
}


bool CCommon::TryFindEquipValue(const CString& equipIniPath, const CString& dataId, CString& outValue)
{
	if (dataId.Left(3) != _T("EQ_"))
		return false;

	CString key = dataId.Mid(3); // EQ_ 제거

	// EquipData.ini에서 키가 있을 수 있는 섹션들 (필요시 추가)
	static const CString sections[] = {
		_T("EQUIPMENT"),
		_T("OPTION"),
		_T("TRAY"),
		_T("PITCH"),
		_T("DELAY_TIME"),
		_T("TOWER"),
		_T("BUZZER"),
		_T("HIDDEN"),
		_T("DRY_RUN"),
		_T("DAY_TOTAL"),
		_T("NOTUSE"),
		_T("TIME_OVER"),
	};

	for (int i = 0; i < (int)(sizeof(sections)/sizeof(sections[0])); ++i)
	{
		CString v = ReadIniString(equipIniPath, sections[i], key);
		if (!v.IsEmpty())
		{
			if(v=="TRUE")
			{
				v="T";

			}
			if(v=="FALSE")
			{
				v="F";

			}
			outValue = v;
			return true;
		}
	}
	return false;
}

bool CCommon::IniKeyExists(const CString& iniPath, const CString& section, const CString& key)
{
	TCHAR buf[2] = {0};
	::GetPrivateProfileString(section, key, _T(""), buf, 2, iniPath);
	return (buf[0] != 0);
}


CString CCommon::StripNumberPrefix(const CString& section)
{
	// "11_LOAD_STAGE_Y1" -> "LOAD_STAGE_Y1"
	int pos = section.Find(_T('_'));
	if (pos < 0) return section;

	// 앞부분이 숫자인지 확인
	CString head = section.Left(pos);
	for (int i=0;i<head.GetLength();++i)
		if (head[i] < _T('0') || head[i] > _T('9'))
			return section; // 숫자 아닌 경우 그대로

	return section.Mid(pos+1);
}

std::map<CString, CString>& CCommon::GetMoveKeyRuleMap()
{
	static std::map<CString, CString> s_map;
	if (!s_map.empty())
		return s_map;

	// ===== LOAD_STAGE_Y1 / Y2 : LOAD, UNLOAD =====
	s_map[_T("LOAD_STAGE_Y1|LOAD")]   = _T("00");
	s_map[_T("LOAD_STAGE_Y1|UNLOAD")] = _T("02");
	s_map[_T("LOAD_STAGE_Y2|LOAD")]   = _T("00");
	s_map[_T("LOAD_STAGE_Y2|UNLOAD")] = _T("02");

	// ===== GOOD_STAGE_Y1 / Y2 : LOAD, ALIGN =====
	s_map[_T("GOOD_STAGE_Y1|LOAD")]  = _T("00");
	s_map[_T("GOOD_STAGE_Y1|ALIGN")] = _T("06");
	s_map[_T("GOOD_STAGE_Y2|LOAD")]  = _T("00");
	s_map[_T("GOOD_STAGE_Y2|ALIGN")] = _T("06");

	// ===== NG_STAGE_Y1 / Y2 : LOAD, ALIGN =====
	s_map[_T("NG_STAGE_Y1|LOAD")]  = _T("00");
	s_map[_T("NG_STAGE_Y1|ALIGN")] = _T("06");
	s_map[_T("NG_STAGE_Y2|LOAD")]  = _T("00");
	s_map[_T("NG_STAGE_Y2|ALIGN")] = _T("06");

	// ===== LOAD_PICKER_* : LOAD_STAGE_1, LOAD_STAGE_2, BOTTOM, ALIGN =====
	// X1
	s_map[_T("LOAD_PICKER_X1|LOAD_STAGE_1")] = _T("01");
	s_map[_T("LOAD_PICKER_X1|LOAD_STAGE_2")] = _T("02");
	s_map[_T("LOAD_PICKER_X1|BOTTOM")]       = _T("03");
	s_map[_T("LOAD_PICKER_X1|ALIGN")]        = _T("04");
	// Y1
	s_map[_T("LOAD_PICKER_Y1|LOAD_STAGE_1")] = _T("01");
	s_map[_T("LOAD_PICKER_Y1|LOAD_STAGE_2")] = _T("02");
	s_map[_T("LOAD_PICKER_Y1|BOTTOM")]       = _T("03");
	s_map[_T("LOAD_PICKER_Y1|ALIGN")]        = _T("04");
	// Z1
	s_map[_T("LOAD_PICKER_Z1|LOAD_STAGE_1")] = _T("01");
	s_map[_T("LOAD_PICKER_Z1|LOAD_STAGE_2")] = _T("02");
	s_map[_T("LOAD_PICKER_Z1|BOTTOM")]       = _T("03");
	s_map[_T("LOAD_PICKER_Z1|ALIGN")]        = _T("04");
	// P1
	s_map[_T("LOAD_PICKER_P1|LOAD_STAGE_1")] = _T("01");
	s_map[_T("LOAD_PICKER_P1|LOAD_STAGE_2")] = _T("02");
	s_map[_T("LOAD_PICKER_P1|BOTTOM")]       = _T("03");
	s_map[_T("LOAD_PICKER_P1|ALIGN")]        = _T("04");

	// X2
	s_map[_T("LOAD_PICKER_X2|LOAD_STAGE_1")] = _T("01");
	s_map[_T("LOAD_PICKER_X2|LOAD_STAGE_2")] = _T("02");
	s_map[_T("LOAD_PICKER_X2|BOTTOM")]       = _T("03");
	s_map[_T("LOAD_PICKER_X2|ALIGN")]        = _T("04");
	// Y2
	s_map[_T("LOAD_PICKER_Y2|LOAD_STAGE_1")] = _T("01");
	s_map[_T("LOAD_PICKER_Y2|LOAD_STAGE_2")] = _T("02");
	s_map[_T("LOAD_PICKER_Y2|BOTTOM")]       = _T("03");
	s_map[_T("LOAD_PICKER_Y2|ALIGN")]        = _T("04");
	// Z2
	s_map[_T("LOAD_PICKER_Z2|LOAD_STAGE_1")] = _T("01");
	s_map[_T("LOAD_PICKER_Z2|LOAD_STAGE_2")] = _T("02");
	s_map[_T("LOAD_PICKER_Z2|BOTTOM")]       = _T("03");
	s_map[_T("LOAD_PICKER_Z2|ALIGN")]        = _T("04");
	// P2
	s_map[_T("LOAD_PICKER_P2|LOAD_STAGE_1")] = _T("01");
	s_map[_T("LOAD_PICKER_P2|LOAD_STAGE_2")] = _T("02");
	s_map[_T("LOAD_PICKER_P2|BOTTOM")]       = _T("03");
	s_map[_T("LOAD_PICKER_P2|ALIGN")]        = _T("04");

	// ===== INSPECT_STAGE_X1~X4 : TOP1, TOP2, UNLOAD =====
	s_map[_T("INSPECT_STAGE_X1|TOP1")]   = _T("01");
	s_map[_T("INSPECT_STAGE_X1|TOP2")]   = _T("02");
	s_map[_T("INSPECT_STAGE_X1|UNLOAD")] = _T("03");

	s_map[_T("INSPECT_STAGE_X2|TOP1")]   = _T("01");
	s_map[_T("INSPECT_STAGE_X2|TOP2")]   = _T("02");
	s_map[_T("INSPECT_STAGE_X2|UNLOAD")] = _T("03");

	s_map[_T("INSPECT_STAGE_X3|TOP1")]   = _T("01");
	s_map[_T("INSPECT_STAGE_X3|TOP2")]   = _T("02");
	s_map[_T("INSPECT_STAGE_X3|UNLOAD")] = _T("03");

	s_map[_T("INSPECT_STAGE_X4|TOP1")]   = _T("01");
	s_map[_T("INSPECT_STAGE_X4|TOP2")]   = _T("02");
	s_map[_T("INSPECT_STAGE_X4|UNLOAD")] = _T("03");

	// ===== UNLOAD_PICKER_X1/X2 : INSPECTION_STAGE_1~4 =====
	// X는 실제 ini에서 05~08이 inspection stage 1~4로 쓰이는 패턴 (Excel MD_ 기준)
	s_map[_T("UNLOAD_PICKER_X1|INSPECTION_STAGE_1")] = _T("05");
	s_map[_T("UNLOAD_PICKER_X1|INSPECTION_STAGE_2")] = _T("06");
	s_map[_T("UNLOAD_PICKER_X1|INSPECTION_STAGE_3")] = _T("07");
	s_map[_T("UNLOAD_PICKER_X1|INSPECTION_STAGE_4")] = _T("08");

	s_map[_T("UNLOAD_PICKER_X2|INSPECTION_STAGE_1")] = _T("05");
	s_map[_T("UNLOAD_PICKER_X2|INSPECTION_STAGE_2")] = _T("06");
	s_map[_T("UNLOAD_PICKER_X2|INSPECTION_STAGE_3")] = _T("07");
	s_map[_T("UNLOAD_PICKER_X2|INSPECTION_STAGE_4")] = _T("08");

	// ===== UNLOAD_PICKER_Y1/Y2/Z1/Z2 : INSPECTION_STAGE_1~4, GOOD_STAGE_1~2, NG_STAGE_1~2 =====
	// Y/Z는 01~04가 inspection 1~4, 05~06 good 1~2, 07~08 ng 1~2 패턴 (Excel MD_ 기준)
#define ADD_UNLOAD_YZ_RULES(CANON) \
	s_map[_T(CANON "|INSPECTION_STAGE_1")] = _T("01"); \
	s_map[_T(CANON "|INSPECTION_STAGE_2")] = _T("02"); \
	s_map[_T(CANON "|INSPECTION_STAGE_3")] = _T("03"); \
	s_map[_T(CANON "|INSPECTION_STAGE_4")] = _T("04"); \
	s_map[_T(CANON "|GOOD_STAGE_1")]       = _T("05"); \
	s_map[_T(CANON "|GOOD_STAGE_2")]       = _T("06"); \
	s_map[_T(CANON "|NG_STAGE_1")]         = _T("07"); \
	s_map[_T(CANON "|NG_STAGE_2")]         = _T("08");

	ADD_UNLOAD_YZ_RULES("UNLOAD_PICKER_Y1");
	ADD_UNLOAD_YZ_RULES("UNLOAD_PICKER_Y2");
	ADD_UNLOAD_YZ_RULES("UNLOAD_PICKER_Z1");
	ADD_UNLOAD_YZ_RULES("UNLOAD_PICKER_Z2");

#undef ADD_UNLOAD_YZ_RULES

	// ===== UNLOAD_PICKER_P1/P2 : GOOD_STAGE, INSPECTION_STAGE, NG_STAGE =====
	// P는 3상태만 쓰이므로 01/02/03 매핑 (Excel MD_ 기준)
	s_map[_T("UNLOAD_PICKER_P1|GOOD_STAGE")]       = _T("01");
	s_map[_T("UNLOAD_PICKER_P1|INSPECTION_STAGE")] = _T("02");
	s_map[_T("UNLOAD_PICKER_P1|NG_STAGE")]         = _T("03");

	s_map[_T("UNLOAD_PICKER_P2|GOOD_STAGE")]       = _T("01");
	s_map[_T("UNLOAD_PICKER_P2|INSPECTION_STAGE")] = _T("02");
	s_map[_T("UNLOAD_PICKER_P2|NG_STAGE")]         = _T("03");

	// ===== TOP1_* : VISION, ANGLE =====
	s_map[_T("TOP1_LIGHT_Z|VISION")] = _T("01");
	s_map[_T("TOP1_LIGHT_Z|ANGLE")]  = _T("02");

	s_map[_T("TOP1_ANGLE_Y|VISION")] = _T("01");
	s_map[_T("TOP1_ANGLE_Y|ANGLE")]  = _T("02");

	s_map[_T("TOP1_VISION_Z|VISION")] = _T("01");
	s_map[_T("TOP1_VISION_Z|ANGLE")]  = _T("02");

	// ===== TOP2_VISION_Z : VISION only =====
	s_map[_T("TOP2_VISION_Z|VISION")] = _T("01");

	return s_map;
}


// canonSection + suffix로 iniKey 반환 (겹침 해결)
bool CCommon::ResolveMoveKeyBySectionAndSuffix(const CString& canonSection,const CString& suffix,CString& outIniKey)
{
	CString k;
	k.Format(_T("%s|%s"), canonSection.GetString(), suffix.GetString());

	const auto& m = GetMoveKeyRuleMap();
	auto it = m.find(k);
	if (it == m.end())
		return false;

	outIniKey = it->second;
	return true;
}


// 대표적인 suffix -> ini key 규칙(필요시 확장)
bool CCommon::ResolveMoveKeyFallbackByExistingKey(
	const CString& moveIniPath,
	const CString& bestSection,    // 실제 섹션명 (숫자 포함 가능)
	const CString& canonSection,   // StripNumberPrefix(bestSection)
	const CString& suffix,
	CString& outIniKey
	)
{
	{
		CString k;
		k.Format(_T("%s|%s"), canonSection.GetString(), suffix.GetString());
		const auto& m = GetMoveKeyRuleMap();
		auto it = m.find(k);
		if (it != m.end())
		{
			if (IniKeyExists(moveIniPath, bestSection, it->second))
			{
				outIniKey = it->second;
				return true;
			}
		}
	}

	// 1) LOAD_PICKER_* 공통 (LOAD_STAGE_1/2/BOTTOM/ALIGN)
	if (suffix == _T("LOAD_STAGE_1"))
	{
		if (IniKeyExists(moveIniPath, bestSection, _T("01"))) { outIniKey = _T("01"); return true; }
	}
	if (suffix == _T("LOAD_STAGE_2"))
	{
		if (IniKeyExists(moveIniPath, bestSection, _T("02"))) { outIniKey = _T("02"); return true; }
	}
	if (suffix == _T("BOTTOM"))
	{
		if (IniKeyExists(moveIniPath, bestSection, _T("03"))) { outIniKey = _T("03"); return true; }
	}
	if (suffix == _T("ALIGN"))
	{
		// stage Y들은 ALIGN이 06인 케이스가 있으므로 06 우선, 없으면 04
		if (IniKeyExists(moveIniPath, bestSection, _T("06"))) { outIniKey = _T("06"); return true; }
		if (IniKeyExists(moveIniPath, bestSection, _T("04"))) { outIniKey = _T("04"); return true; }
	}

	// 2) LOAD/UNLOAD
	if (suffix == _T("LOAD"))
	{
		if (IniKeyExists(moveIniPath, bestSection, _T("00"))) { outIniKey = _T("00"); return true; }
	}
	if (suffix == _T("UNLOAD"))
	{
		if (IniKeyExists(moveIniPath, bestSection, _T("02"))) { outIniKey = _T("02"); return true; }
		if (IniKeyExists(moveIniPath, bestSection, _T("03"))) { outIniKey = _T("03"); return true; }
	}

	// 3) INSPECT_STAGE_X* : TOP1/TOP2/UNLOAD
	if (suffix == _T("TOP1"))
	{
		if (IniKeyExists(moveIniPath, bestSection, _T("01"))) { outIniKey = _T("01"); return true; }
	}
	if (suffix == _T("TOP2"))
	{
		if (IniKeyExists(moveIniPath, bestSection, _T("02"))) { outIniKey = _T("02"); return true; }
	}

	// 4) VISION/ANGLE
	if (suffix == _T("VISION"))
	{
		if (IniKeyExists(moveIniPath, bestSection, _T("00"))) { outIniKey = _T("00"); return true; }
		if (IniKeyExists(moveIniPath, bestSection, _T("01"))) { outIniKey = _T("01"); return true; }
	}
	if (suffix == _T("ANGLE"))
	{
		if (IniKeyExists(moveIniPath, bestSection, _T("02"))) { outIniKey = _T("02"); return true; }
		if (IniKeyExists(moveIniPath, bestSection, _T("03"))) { outIniKey = _T("03"); return true; }
	}

	// 5) UNLOAD_PICKER_Y/Z : INSPECTION_STAGE_n / GOOD_STAGE_n / NG_STAGE_n
	//    (suffix에 "_1~_4" 파싱해서 계산)
	{
		// INSPECTION_STAGE_#
		const CString p1 = _T("INSPECTION_STAGE_");
		const CString p2 = _T("GOOD_STAGE_");
		const CString p3 = _T("NG_STAGE_");

		if (suffix.Left(p1.GetLength()) == p1)
		{
			int n = _ttoi(suffix.Mid(p1.GetLength()));
			if (n >= 1 && n <= 4)
			{
				// X 계열이면 05~08, Y/Z면 01~04
				CString key;
				if (canonSection.Left(14) == _T("UNLOAD_PICKER_X"))
					key.Format(_T("%02d"), 4 + n);   // 05~08
				else
					key.Format(_T("%02d"), n);       // 01~04

				if (IniKeyExists(moveIniPath, bestSection, key)) { outIniKey = key; return true; }
			}
		}
		if (suffix.Left(p2.GetLength()) == p2)
		{
			int n = _ttoi(suffix.Mid(p2.GetLength()));
			if (n >= 1 && n <= 2)
			{
				CString key;
				key.Format(_T("%02d"), 4 + n); // 05~06
				if (IniKeyExists(moveIniPath, bestSection, key)) { outIniKey = key; return true; }
			}
		}
		if (suffix.Left(p3.GetLength()) == p3)
		{
			int n = _ttoi(suffix.Mid(p3.GetLength()));
			if (n >= 1 && n <= 2)
			{
				CString key;
				key.Format(_T("%02d"), 6 + n); // 07~08
				if (IniKeyExists(moveIniPath, bestSection, key)) { outIniKey = key; return true; }
			}
		}
	}

	// 6) UNLOAD_PICKER_P1/P2 : GOOD_STAGE / INSPECTION_STAGE / NG_STAGE
	if (suffix == _T("GOOD_STAGE"))
	{
		if (IniKeyExists(moveIniPath, bestSection, _T("01"))) { outIniKey = _T("01"); return true; }
	}
	if (suffix == _T("INSPECTION_STAGE"))
	{
		if (IniKeyExists(moveIniPath, bestSection, _T("02"))) { outIniKey = _T("02"); return true; }
	}
	if (suffix == _T("NG_STAGE"))
	{
		if (IniKeyExists(moveIniPath, bestSection, _T("03"))) { outIniKey = _T("03"); return true; }
	}

	return false;
}
bool CCommon::TryFindMoveValue(const CString& moveIniPath, const CString& dataId, CString& outValue)
{
	if (dataId.Left(3) != _T("MD_"))
		return false;

	CString rest = dataId.Mid(3); // MD_ 제거

	// 1) MoveData.ini 섹션 목록
	TCHAR sectionBuf[65535] = {0};
	DWORD n = ::GetPrivateProfileSectionNames(sectionBuf, 65535, moveIniPath);
	if (n == 0)
		return false;

	// 2) DataID와 가장 잘 맞는 섹션 선택
	CString bestSection;
	int bestLen = -1;

	const TCHAR* p = sectionBuf;
	while (*p)
	{
		CString sec(p);
		CString canon = StripNumberPrefix(sec);

		if (rest.Find(canon + _T("_")) == 0 || rest == canon)
		{
			if (canon.GetLength() > bestLen)
			{
				bestLen = canon.GetLength();
				bestSection = sec;
			}
		}
		p += sec.GetLength() + 1;
	}

	if (bestSection.IsEmpty())
		return false;

	// 3) canonSection + suffix 계산
	CString canonBest = StripNumberPrefix(bestSection);
	CString suffix;

	if (rest.GetLength() > canonBest.GetLength() + 1)
		suffix = rest.Mid(canonBest.GetLength() + 1);
	else
		suffix = _T("");

	// 4) iniKey 결정
	CString iniKey;

	// (1) 우선: 완전 고정 룰
	if (!ResolveMoveKeyBySectionAndSuffix(canonBest, suffix, iniKey))
	{
		// (2) fallback: 실제 ini에 존재하는 key로 판단
		if (!ResolveMoveKeyFallbackByExistingKey(
			moveIniPath,
			bestSection,   // 실제 섹션명
			canonBest,     // 숫자 제거 섹션명
			suffix,
			iniKey))
		{
			return false;
		}
	}

	// 5) 값 읽기
	CString value = ReadIniString(moveIniPath, bestSection, iniKey);
	if (value.IsEmpty())
		return false;

	if(value=="TRUE")
			{
				value="T";

			}
			if(value=="FALSE")
			{
				value="F";

			}
	outValue = value;
	return true;
}



bool CCommon::BuildDataIdValueVector(const CString& equipIniPath, const CString& moveIniPath,vectorPair& outVec)
{
	outVec.clear();
	outVec.reserve(nHandlerDataIdCount);

	for (int i = 0; i < nHandlerDataIdCount; ++i)
	{
		CString dataId = strHandlerDataIds[i];
		CString value;

		bool ok = false;
		if (dataId.Left(3) == _T("EQ_"))
			ok = TryFindEquipValue(equipIniPath, dataId, value);
		else if (dataId.Left(3) == _T("MD_"))
			ok = TryFindMoveValue(moveIniPath, dataId, value);

		// 못 찾는 경우 빈값으로 넣거나, 로그 남기기 선택
		outVec.push_back(std::make_pair(dataId, ok ? value : _T("")));
	}

	return true;
}