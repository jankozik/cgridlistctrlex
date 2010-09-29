#include "stdafx.h"
#pragma warning(disable:4100)	// unreferenced formal parameter

#include "CGridColumnManagerProfile.h"

#include "CGridListCtrlEx.h"
#include "CGridColumnTrait.h"

//------------------------------------------------------------------------
//! CGridColumnManagerProfile - Constructor
//!
//! @param strViewName Name to identify and persist the configuration
//------------------------------------------------------------------------
CGridColumnManagerProfile::CGridColumnManagerProfile(const CString& strViewName)
{
	m_ApplyingConfiguration = false;
	m_pColumnConfig = new CGridColumnConfigWinApp(strViewName);
}

//------------------------------------------------------------------------
//! CGridColumnManagerProfile - Constructor
//!
//! @param pColumnConfig Interface for persisting the configuration
//------------------------------------------------------------------------
CGridColumnManagerProfile::CGridColumnManagerProfile(CGridColumnConfigProfiles* pColumnConfig)
{
	m_ApplyingConfiguration = false;
	m_pColumnConfig = pColumnConfig;
}

//------------------------------------------------------------------------
//! CGridColumnManagerProfile - Destructor
//------------------------------------------------------------------------
CGridColumnManagerProfile::~CGridColumnManagerProfile()
{
	delete m_pColumnConfig;
	m_pColumnConfig = NULL;
}

//------------------------------------------------------------------------
//! Saves the column configuration of the list control
//!
//! @param owner The list control
//! @param config The interface for persisting the configuration
//------------------------------------------------------------------------
void CGridColumnManagerProfile::SaveConfiguration(CGridListCtrlEx& owner, CGridColumnConfig& config)
{
	if (m_ApplyingConfiguration)
		return;

	config.RemoveCurrentConfig();	// Reset the existing config

	int nColCount = owner.GetColumnCount();
	int* pOrderArray = new int[nColCount];
	owner.GetColumnOrderArray(pOrderArray, nColCount);

	int nVisibleCols = 0;
	for(int i = 0 ; i < nColCount; ++i)
	{
		int nCol = pOrderArray[i];
		int nColData = owner.GetColumnData(nCol);

		if (owner.IsColumnVisible(nCol))
		{
			CString colSetting;
			colSetting.Format(_T("ColumnData_%d"), nVisibleCols);
			config.SetIntSetting(colSetting, nColData);

			SaveColumnConfiguration(nVisibleCols, nCol, owner, config);

			nVisibleCols++;
		}
	}
	config.SetIntSetting(_T("ColumnCount"), nVisibleCols);

	delete [] pOrderArray;
}

//------------------------------------------------------------------------
//! Loads and applies the column configuration for the list control
//!
//! @param owner The list control
//! @param config The interface for persisting the configuration
//------------------------------------------------------------------------
void CGridColumnManagerProfile::LoadConfiguration(CGridListCtrlEx& owner, CGridColumnConfig& config)
{
	if (!m_pColumnConfig->HasDefaultConfig())
	{
		// Validate that column data is setup correctly
		CSimpleMap<int,int> uniqueChecker;
		for(int nCol = 0; nCol < owner.GetColumnCount(); ++nCol)
		{
			if (owner.IsColumnAlwaysHidden(nCol))
				continue;
			int nColData = owner.GetColumnData(nCol);
			ASSERT(uniqueChecker.FindKey(nColData)==-1);
			uniqueChecker.Add(nColData,nCol);
		}

		SaveConfiguration(owner, m_pColumnConfig->GetDefaultConfig());
	}

	m_ApplyingConfiguration = true;

	int nVisibleCols = config.GetIntSetting(_T("ColumnCount"));

	int nColCount = owner.GetColumnCount();
	int* pOrderArray = new int[nColCount];
	owner.GetColumnOrderArray(pOrderArray, nColCount);

	owner.SetRedraw(FALSE);

	// All invisible columns must be place in the begining of the order-array
	int nColOrder = nColCount;
	for(int i = nVisibleCols-1; i >= 0; --i)
	{
		CString colSetting;
		colSetting.Format(_T("ColumnData_%d"), i);
		int nColData = config.GetIntSetting(colSetting);
		for(int nCol = 0; nCol < nColCount; ++nCol)
		{
			// Check if already in array
			bool alreadyIncluded = false;
			for(int j = nColOrder; j < nColCount; ++j)
				if (pOrderArray[j]==nCol)
				{
					alreadyIncluded = true;
					break;
				}
			if (alreadyIncluded)
				continue;

			if (nColData==owner.GetColumnData(nCol))
			{
				// Column still exists
				if (owner.IsColumnAlwaysHidden(nCol))
					continue;

				CGridColumnTrait::ColumnState& columnState = owner.GetColumnTrait(nCol)->GetColumnState();
				columnState.m_Visible = true;
				LoadColumnConfiguration(i, nCol, owner, config);
				pOrderArray[--nColOrder] = nCol;
				break;
			}
		}
	}

	// Are there any always visible columns, that we must ensure are visible ?
	for(int nCol = nColCount-1; nCol >= 0; --nCol)
	{
		if (!owner.IsColumnAlwaysVisible(nCol))
			continue;

		bool visible = false;
		for(int i = nColOrder; i < nColCount; ++i)
		{
			if (pOrderArray[i]==nCol)
			{
				visible = true;
				break;
			}
		}
		if (!visible)
		{
			CGridColumnTrait::ColumnState& columnState = owner.GetColumnTrait(nCol)->GetColumnState();
			columnState.m_Visible = true;
			pOrderArray[--nColOrder] = nCol;
		}
	}

	// Did we find any visible columns in the saved configuration ?
	if (nColOrder < nColCount)
	{
		// All remaining columns are marked as invisible
		while(nColOrder > 0)
		{
			// Find nCol som ikke er i array
			int nCol = -1;
			for(nCol = nColCount-1; nCol >= 0; --nCol)
			{
				bool visible = false;
				for(int j = nColOrder; j < nColCount; ++j)
				{
					if (pOrderArray[j]==nCol)
					{
						visible = true;
						break;
					}
				}
				if (!visible)
					break;
			}
			ASSERT(nCol!=-1);
			CGridColumnTrait::ColumnState& columnState = owner.GetColumnTrait(nCol)->GetColumnState();
			columnState.m_OrgPosition = -1;
			columnState.m_OrgWidth = owner.GetColumnWidth(nCol);
			owner.SetColumnWidth(nCol, 0);
			columnState.m_Visible = false;
			ASSERT(nColOrder>0);
			pOrderArray[--nColOrder] = nCol;
		}

		// Only update the column configuration if there are visible columns
		ASSERT(nColOrder==0);	// All entries in the order-array must be set
		owner.SetColumnOrderArray(nColCount, pOrderArray);
	}

	delete [] pOrderArray;

	m_ApplyingConfiguration = false;

	owner.SetRedraw(TRUE);
	owner.Invalidate(TRUE);
	owner.UpdateWindow();
}

//------------------------------------------------------------------------
//! Saves the column configuration of a single column
//!
//! @param nConfigCol The column index in the persisting interface
//! @param nOwnerCol The column index in the owner list control
//! @param owner The list control
//! @param config The interface for persisting the configuration
//------------------------------------------------------------------------
void CGridColumnManagerProfile::SaveColumnConfiguration(int nConfigCol, int nOwnerCol, CGridListCtrlEx& owner, CGridColumnConfig& config)
{
	CString colSetting;
	colSetting.Format(_T("ColumnWidth_%d"), nConfigCol);
	config.SetIntSetting(colSetting, owner.GetColumnWidth(nOwnerCol));
}

//------------------------------------------------------------------------
//! Loads the column configuration of a single column
//!
//! @param nConfigCol The column index in the persisting interface
//! @param nOwnerCol The column index in the owner list control
//! @param owner The list control
//! @param config The interface for persisting the configuration
//------------------------------------------------------------------------
void CGridColumnManagerProfile::LoadColumnConfiguration(int nConfigCol, int nOwnerCol, CGridListCtrlEx& owner, CGridColumnConfig& config)
{
	CString colSetting;
	if (owner.IsColumnResizable(nOwnerCol))
	{
		colSetting.Format(_T("ColumnWidth_%d"), nConfigCol);
		int width = config.GetIntSetting(colSetting);
		owner.SetColumnWidth(nOwnerCol, width);
	}
}

//------------------------------------------------------------------------
//! Has the ability to reset the column configuration to its default configuration
//!
//! @param owner The list control with the columns
//! @param strTitle Title to show in the context menu when right-clicking the column
//! @return Default column configuration is available (true / false)
//------------------------------------------------------------------------
bool CGridColumnManagerProfile::HasColumnsDefault(CGridListCtrlEx& owner, CString& strTitle)
{
	strTitle = _T("Reset columns");
	return m_pColumnConfig->HasDefaultConfig();
}

//------------------------------------------------------------------------
//! Reset the column configuration to its default configuration
//!
//! @param owner The list control with the columns
//------------------------------------------------------------------------
void CGridColumnManagerProfile::ResetColumnsDefault(CGridListCtrlEx& owner)
{
	m_pColumnConfig->ResetConfigDefault();
	LoadConfiguration(owner, *m_pColumnConfig);
}

//------------------------------------------------------------------------
//! Adds a profile to the official list of column configuration profiles
//!
//! @param strProfile Name of the profile
//------------------------------------------------------------------------
void CGridColumnManagerProfile::AddColumnProfile(const CString& strProfile)
{
	m_pColumnConfig->AddProfile(strProfile);
}

//------------------------------------------------------------------------
//! Removes a profile from the official list of column configuration profiles
//!
//! @param strProfile Name of the profile
//------------------------------------------------------------------------
void CGridColumnManagerProfile::DeleteColumnProfile(const CString& strProfile)
{
	m_pColumnConfig->DeleteProfile(strProfile);
}

//------------------------------------------------------------------------
//! Can switch between multiple column configurations
//!
//! @param owner The list control with the columns
//! @param profiles List of available column profiles
//! @param strTitle Title to show in the context menu when right-clicking the column
//! @return Name of the current column profile
//------------------------------------------------------------------------
CString CGridColumnManagerProfile::HasColumnProfiles(CGridListCtrlEx& owner, CSimpleArray<CString>& profiles, CString& strTitle)
{
	strTitle = _T("Column Profiles");
	m_pColumnConfig->GetProfiles(profiles);
	return m_pColumnConfig->GetActiveProfile();
}

//------------------------------------------------------------------------
//! Switch to different column configurations profile
//!
//! @param owner The list control with the columns
//! @param strProfile List of available column profiles
//------------------------------------------------------------------------
void CGridColumnManagerProfile::SwichColumnProfile(CGridListCtrlEx& owner, const CString& strProfile)
{
	// Save the current configuration before switching to the new one
	SaveConfiguration(owner, *m_pColumnConfig);
	m_pColumnConfig->SetActiveProfile(strProfile);
	LoadConfiguration(owner, *m_pColumnConfig);
}

//------------------------------------------------------------------------
//! Called when the column configuration is setup for the list control
//!
//! @param owner The list control with the columns
//------------------------------------------------------------------------
void CGridColumnManagerProfile::OnColumnSetup(CGridListCtrlEx& owner)
{
	LoadConfiguration(owner, *m_pColumnConfig);
}

//------------------------------------------------------------------------
//! Called when the list control looses focus to another control
//!
//! @param owner The list control with the columns
//------------------------------------------------------------------------
void CGridColumnManagerProfile::OnOwnerKillFocus(CGridListCtrlEx& owner)
{
	SaveConfiguration(owner, *m_pColumnConfig);
}

//------------------------------------------------------------------------
//! Called after a column has been resized
//!
//! @param owner The list control with the columns
//------------------------------------------------------------------------
void CGridColumnManagerProfile::OnColumnResize(CGridListCtrlEx& owner)
{
	SaveConfiguration(owner, *m_pColumnConfig);
}

//------------------------------------------------------------------------
//! Called after a column has been added / removed
//!
//! @param owner The list control with the columns
//------------------------------------------------------------------------
void CGridColumnManagerProfile::OnColumnPick(CGridListCtrlEx& owner)
{
	SaveConfiguration(owner, *m_pColumnConfig);
}

//------------------------------------------------------------------------
//! CGridColumnConfig - Constructor
//!
//! @param strViewName Name to identify and persist the configuration
//------------------------------------------------------------------------
CGridColumnConfig::CGridColumnConfig(const CString& strViewName)
	:m_ViewName(strViewName)
{
}

//------------------------------------------------------------------------
//! CGridColumnConfig - Destructor
//------------------------------------------------------------------------
CGridColumnConfig::~CGridColumnConfig()
{
}

//------------------------------------------------------------------------
//! Retrieves a setting value for the view
//!
//! @param strName Name of setting
//! @param strDefval Default value to return if no value was found
//! @return Value of the setting
//------------------------------------------------------------------------
CString CGridColumnConfig::GetSetting(const CString& strName, const CString& strDefval) const
{
	return ReadSetting(GetSectionName(), strName, strDefval);
}

//------------------------------------------------------------------------
//! Updates a setting value for the view
//!
//! @param strName Name of setting
//! @param strValue New setting value
//------------------------------------------------------------------------
void CGridColumnConfig::SetSetting(const CString& strName, const CString& strValue)
{
	WriteSetting(GetSectionName(), strName, strValue);
}

//------------------------------------------------------------------------
//! Retrieves the current section name to store the settings
//!
//! @return Current section name
//------------------------------------------------------------------------
const CString& CGridColumnConfig::GetSectionName() const
{
	return m_ViewName;
}

//------------------------------------------------------------------------
//! Removes the current configuration
//------------------------------------------------------------------------
void CGridColumnConfig::RemoveCurrentConfig()
{
	RemoveSection(GetSectionName());
}

//------------------------------------------------------------------------
//! Retrieves a bool setting value for the view
//!
//! @param strName Name of setting
//! @param bDefval Default value to return if no value was found
//! @return Value of the setting
//------------------------------------------------------------------------
bool CGridColumnConfig::GetBoolSetting(const CString& strName, bool bDefval) const
{
	const CString& strValue = GetSetting(strName, ConvertBoolSetting(bDefval));
	if (strValue==_T("TRUE"))
		return true;
	else
	if (strValue==_T("FALSE"))
		return false;
	else
		return bDefval;
}

//------------------------------------------------------------------------
//! Converts a bool setting to a string value
//!
//! @param bValue The setting value
//! @return The setting value as string
//------------------------------------------------------------------------
CString CGridColumnConfig::ConvertBoolSetting(bool bValue) const
{
	return bValue ? _T("TRUE") : _T("FALSE");
}

//------------------------------------------------------------------------
//! Updates the value of a bool setting
//!
//! @param strName The setting name
//! @param bValue The new setting value
//------------------------------------------------------------------------
void CGridColumnConfig::SetBoolSetting(const CString& strName, bool bValue)
{
	SetSetting(strName, ConvertBoolSetting(bValue));
}

//------------------------------------------------------------------------
//! Retrieves an integer setting value for the view
//!
//! @param strName Name of setting
//! @param nDefval Default value to return if no value was found
//! @return Value of the setting
//------------------------------------------------------------------------
int CGridColumnConfig::GetIntSetting(const CString& strName, int nDefval) const
{
	const CString& value = GetSetting(strName, ConvertIntSetting(nDefval));
	return _ttoi(value);
}

//------------------------------------------------------------------------
//! Converts an integer setting to a string value
//!
//! @param nValue The setting value
//! @return The setting value as string
//------------------------------------------------------------------------
CString CGridColumnConfig::ConvertIntSetting(int nValue) const
{
	CString strValue;
	strValue.Format(_T("%d"), nValue);
	return strValue;
}

//------------------------------------------------------------------------
//! Updates the value of an integer setting
//!
//! @param strName The setting name
//! @param nValue The new setting value
//------------------------------------------------------------------------
void CGridColumnConfig::SetIntSetting(const CString& strName, int nValue)
{
	SetSetting(strName, ConvertIntSetting(nValue));
}

//------------------------------------------------------------------------
//! Retrieves a float setting value for the view
//!
//! @param strName Name of setting
//! @param nDefval Default value to return if no value was found
//! @return Value of the setting
//------------------------------------------------------------------------
double CGridColumnConfig::GetFloatSetting(const CString& strName, double nDefval) const
{
	const CString& value = GetSetting(strName, ConvertFloatSetting(nDefval));
#ifdef _UNICODE
	return wcstod(value, NULL);
#else
	return strtod(value, NULL);
#endif
}

//------------------------------------------------------------------------
//! Converts a float setting to a string value
//!
//! @param nValue The setting value
//! @param nDecimals The number of decimals to persist
//! @return The setting value as string
//------------------------------------------------------------------------
CString CGridColumnConfig::ConvertFloatSetting(double nValue, int nDecimals) const
{
	CString strFormat;
	strFormat.Format(_T("%%.%df"), nDecimals);

	CString strValue;	
	strValue.Format(strFormat, nValue);
	return strValue;
}

//------------------------------------------------------------------------
//! Updates the value of a float setting
//!
//! @param strName The setting name
//! @param nValue The new setting value
//! @param nDecimals The number of decimals to persist
//------------------------------------------------------------------------
void CGridColumnConfig::SetFloatSetting(const CString& strName, double nValue, int nDecimals)
{
	SetSetting(strName, ConvertFloatSetting(nValue, nDecimals));
}

//------------------------------------------------------------------------
//! Splits a delimited string into a string-array
//!
//! @param strArray The delimited string 
//! @param values The string array
//! @param strDelimiter The delimiter
//------------------------------------------------------------------------
void CGridColumnConfig::SplitArraySetting(const CString& strArray, CSimpleArray<CString>& values, const CString& strDelimiter) const
{
	// Perform tokenize using strDelimiter
	int cur_pos = 0;
	int prev_pos = 0;
	int length = strArray.GetLength();
	while(cur_pos < length)
	{
		cur_pos = strArray.Find(strDelimiter, prev_pos);
		if (cur_pos==-1)
		{
			CString value = strArray.Mid(prev_pos, length - prev_pos);
			values.Add(value);
			break;
		}
		else
		{
			CString value = strArray.Mid(prev_pos, cur_pos - prev_pos);
			values.Add(value);
			prev_pos = cur_pos + strDelimiter.GetLength();
		}
	}
}

//------------------------------------------------------------------------
//! Retrieves a string-array setting value for the view
//!
//! @param strName Name of setting
//! @param values String-array
//! @param strDelimiter The delimiter for splitting a single string into an array
//------------------------------------------------------------------------
void CGridColumnConfig::GetArraySetting(const CString& strName, CSimpleArray<CString>& values, const CString& strDelimiter) const
{
	const CString& strArray = GetSetting(strName, _T(""));
	if (strArray.IsEmpty())
		return;

	SplitArraySetting(strArray, values, strDelimiter);
}

//------------------------------------------------------------------------
//! Converts a string-array setting value into a delimited string
//!
//! @param values String-array
//! @param strDelimiter The delimiter for combining the values of an array to a string
//! @return The string-array combined into a single string
//------------------------------------------------------------------------
CString CGridColumnConfig::ConvertArraySetting(const CSimpleArray<CString>& values, const CString& strDelimiter) const
{
	CString strValue;
	for(int i = 0; i < values.GetSize() ; ++i)
	{
		if (!strValue.IsEmpty())
			strValue += strDelimiter;
		strValue += values[i];
	}
	return strValue;
}

//------------------------------------------------------------------------
//! Updates the value of a string-array setting
//!
//! @param strName The setting name
//! @param values The new string array
//! @param strDelimiter The delimiter for combining the values of an array to a string
//------------------------------------------------------------------------
void CGridColumnConfig::SetArraySetting(const CString& strName, const CSimpleArray<CString>& values, const CString& strDelimiter)
{
	SetSetting(strName, ConvertArraySetting(values, strDelimiter));
}

//------------------------------------------------------------------------
//! Retrieves a integer-array setting value for the view
//!
//! @param strName Name of setting
//! @param values integer-array
//! @param strDelimiter The delimiter for splitting a single string into an array
//------------------------------------------------------------------------
void CGridColumnConfig::GetArraySetting(const CString& strName, CSimpleArray<int>& values, const CString& strDelimiter) const
{
	CSimpleArray<CString> strArray;
	GetArraySetting(strName, strArray, strDelimiter);
	for(int i = 0 ; i < strArray.GetSize(); ++i)
	{
		int value = _ttoi(strArray[i]);
		values.Add(value);
	}
}

//------------------------------------------------------------------------
//! Converts an integer-array setting value into a delimited string
//!
//! @param values Integer-array
//! @param strDelimiter The delimiter for combining the values of an array to a string
//! @return The string-array combined into a single string
//------------------------------------------------------------------------
CString CGridColumnConfig::ConvertArraySetting(const CSimpleArray<int>& values, const CString& strDelimiter) const
{
	CString strValue;
	CString strArray;
	for(int i = 0; i < values.GetSize(); ++i)
	{
		if (!strArray.IsEmpty())
			strArray += strDelimiter;
		strValue.Format( _T("%d"), values[i]);
		strArray += strValue;
	}
	return strArray;
}

//------------------------------------------------------------------------
//! Updates the value of an integer-array setting
//!
//! @param strName The setting name
//! @param values The integer array
//! @param strDelimiter The delimiter for combining the values of an array to a string
//------------------------------------------------------------------------
void CGridColumnConfig::SetArraySetting(const CString& strName, const CSimpleArray<int>& values, const CString& strDelimiter)
{
	SetSetting(strName, ConvertArraySetting(values, strDelimiter));
}

//------------------------------------------------------------------------
//! Retrieves a font setting value for the view
//!
//! @param strName Name of setting
//! @return Value of the setting
//------------------------------------------------------------------------
LOGFONT CGridColumnConfig::GetLogFontSetting(const CString& strName) const
{
	LOGFONT font = {0};

	CSimpleArray<CString> strArray;
	GetArraySetting(strName, strArray);
	if (strArray.GetSize() != 13)
		return font;

#if __STDC_WANT_SECURE_LIB__
	_tcscpy_s(font.lfFaceName, sizeof(font.lfFaceName)/sizeof(TCHAR), strArray[0]);
#else
	_tcsncpy(font.lfFaceName, strArray[0], sizeof(font.lfFaceName)/sizeof(TCHAR));
#endif
	font.lfHeight = _ttoi(strArray[1]);
	font.lfWidth = _ttoi(strArray[2]);
	font.lfEscapement = _ttoi(strArray[3]);
	font.lfOrientation = _ttoi(strArray[4]);
	font.lfWeight = _ttoi(strArray[5]);
	font.lfItalic = (BYTE)_ttoi(strArray[6]);
	font.lfUnderline = (BYTE)_ttoi(strArray[7]);
	font.lfStrikeOut = (BYTE)_ttoi(strArray[8]);
	font.lfCharSet = (BYTE)_ttoi(strArray[9]);
	font.lfOutPrecision = (BYTE)_ttoi(strArray[10]);
	font.lfQuality = (BYTE)_ttoi(strArray[11]);
	font.lfPitchAndFamily = (BYTE)_ttoi(strArray[12]);
	return font;
}

//------------------------------------------------------------------------
//! Converts a font setting value into a delimited string
//!
//! @param font The setting value
//! @return The delimited string
//------------------------------------------------------------------------
CString CGridColumnConfig::ConvertLogFontSetting(const LOGFONT& font) const
{
	CSimpleArray<CString> strArray;

	CString strValue(font.lfFaceName, sizeof(font.lfFaceName)/sizeof(TCHAR));
	strArray.Add(strValue);
	strValue.Format(_T("%d"), font.lfHeight);
	strArray.Add(strValue);
	strValue.Format(_T("%d"), font.lfWidth);
	strArray.Add(strValue);
	strValue.Format(_T("%d"), font.lfEscapement);
	strArray.Add(strValue);
	strValue.Format(_T("%d"), font.lfOrientation);
	strArray.Add(strValue);
	strValue.Format(_T("%d"), font.lfWeight);
	strArray.Add(strValue);
	strValue.Format(_T("%d"), font.lfItalic);
	strArray.Add(strValue);
	strValue.Format(_T("%d"), font.lfUnderline);
	strArray.Add(strValue);
	strValue.Format(_T("%d"), font.lfStrikeOut);
	strArray.Add(strValue);
	strValue.Format(_T("%d"), font.lfCharSet);
	strArray.Add(strValue);
	strValue.Format(_T("%d"), font.lfOutPrecision);
	strArray.Add(strValue);
	strValue.Format(_T("%d"), font.lfQuality);
	strArray.Add(strValue);
	strValue.Format(_T("%d"), font.lfPitchAndFamily);
	strArray.Add(strValue);

	return ConvertArraySetting(strArray);
}

//------------------------------------------------------------------------
//! Updates the value of a font setting
//!
//! @param strName The setting name
//! @param font The new setting value
//------------------------------------------------------------------------
void CGridColumnConfig::SetLogFontSetting(const CString& strName, const LOGFONT& font)
{
	SetSetting(strName, ConvertLogFontSetting(font));
}

//------------------------------------------------------------------------
//! Retrieves a rectangle setting value for the view
//!
//! @param strName Name of setting
//! @param rectDefval Default value to return if no value was found
//! @return Value of the setting
//------------------------------------------------------------------------
CRect CGridColumnConfig::GetRectSetting(const CString& strName, const CRect& rectDefval) const
{
	CSimpleArray<CString> strArray;
	GetArraySetting(strName, strArray);
	if (strArray.GetSize() != 4)
		return rectDefval;

	CRect rect(0,0,0,0);
    rect.left = _ttoi(strArray[0]);
    rect.top = _ttoi(strArray[1]);
    rect.right = _ttoi(strArray[2]);
    rect.bottom = _ttoi(strArray[3]);
	return rect;
}

//------------------------------------------------------------------------
//! Converts a rectangle setting value into a delimited string
//!
//! @param rect The setting value
//! @return The delimited string
//------------------------------------------------------------------------
CString CGridColumnConfig::ConvertRectSetting(const RECT& rect) const
{
	CSimpleArray<CString> strArray;
	CString strValue;
	strValue.Format(_T("%d"), rect.left);
	strArray.Add(strValue);
	strValue.Format(_T("%d"), rect.top);
	strArray.Add(strValue);
	strValue.Format(_T("%d"), rect.right);
	strArray.Add(strValue);
	strValue.Format(_T("%d"), rect.bottom);
	strArray.Add(strValue);

	return ConvertArraySetting(strArray);
}

//------------------------------------------------------------------------
//! Updates the value of a rectangle setting
//!
//! @param strName The setting name
//! @param rect The new setting value
//------------------------------------------------------------------------
void CGridColumnConfig::SetRectSetting(const CString& strName, const RECT& rect)
{
	SetSetting(strName, ConvertRectSetting(rect));
}

//------------------------------------------------------------------------
//! Retrieves a color setting value for the view
//!
//! @param strName Name of setting
//! @param colorDefval Default value to return if no value was found
//! @return Value of the setting
//------------------------------------------------------------------------
COLORREF CGridColumnConfig::GetColorSetting(const CString& strName, const COLORREF colorDefval) const
{
	CSimpleArray<CString> strArray;
	GetArraySetting(strName, strArray);
	if (strArray.GetSize() != 3)
		return colorDefval;

	int r = _ttoi(strArray[0]);
	int g = _ttoi(strArray[1]);
	int b = _ttoi(strArray[2]);

	return RGB(r,g,b);
}

//------------------------------------------------------------------------
//! Converts a color setting value into a delimited string
//!
//! @param color The setting value
//! @return The delimited string
//------------------------------------------------------------------------
CString CGridColumnConfig::ConvertColorSetting(COLORREF color) const
{
	CSimpleArray<CString> strArray;
	CString strValue;
	strValue.Format(_T("%d"), GetRValue(color));
	strArray.Add(strValue);
	strValue.Format(_T("%d"), GetGValue(color));
	strArray.Add(strValue);
	strValue.Format(_T("%d"), GetBValue(color));
	strArray.Add(strValue);

	return ConvertArraySetting(strArray);
}

//------------------------------------------------------------------------
//! Updates the value of a color setting
//!
//! @param strName The setting name
//! @param color The new setting value
//------------------------------------------------------------------------
void CGridColumnConfig::SetColorSetting(const CString& strName, COLORREF color)
{
	SetSetting(strName, ConvertColorSetting(color));
}

//------------------------------------------------------------------------
//! CGridColumnConfigLocal - Constructor
//!
//! @param strViewName Name to identify and persist the configuration
//------------------------------------------------------------------------
CGridColumnConfigDefault::CGridColumnConfigLocal::CGridColumnConfigLocal(const CString& strViewName)
:CGridColumnConfig(strViewName)
{}

//------------------------------------------------------------------------
//! CGridColumnConfigLocal - Copy constructor to ensure proper
//! copy of m_LocalSettings.
//------------------------------------------------------------------------
CGridColumnConfigDefault::CGridColumnConfigLocal::CGridColumnConfigLocal(const CGridColumnConfigDefault::CGridColumnConfigLocal& other)
:CGridColumnConfig(other)
{
	*this = other;
}

//------------------------------------------------------------------------
//! CGridColumnConfigLocal - Assignment operator to ensure proper
//! assignment of m_LocalSettings.
//------------------------------------------------------------------------
CGridColumnConfigDefault::CGridColumnConfigLocal& CGridColumnConfigDefault::CGridColumnConfigLocal::operator=(const CGridColumnConfigDefault::CGridColumnConfigLocal& other)
{
	if (this==&other)
		return *this;

	static_cast<CGridColumnConfig&>(*this) = other;

	m_LocalSettings.RemoveAll();
	for(int i = 0; i < other.m_LocalSettings.GetSize(); ++i)
		m_LocalSettings.Add(other.m_LocalSettings.GetKeyAt(i), other.m_LocalSettings.GetValueAt(i));
	return *this;
}

//------------------------------------------------------------------------
//! Implements an interface for reading the setting value from memory
//!
//! @param strSection Name of section
//! @param strSetting Name of setting
//! @param strDefval Default value to return if no value was found
//! @return Value of the setting
//------------------------------------------------------------------------
CString CGridColumnConfigDefault::CGridColumnConfigLocal::ReadSetting(const CString& strSection, const CString& strSetting, const CString& strDefval) const
{
	for(int i = 0; i < m_LocalSettings.GetSize(); ++i)
		if (m_LocalSettings.GetKeyAt(i)==strSetting)
			return m_LocalSettings.GetValueAt(i);

	return strDefval;
}

//------------------------------------------------------------------------
//! Implements an interface for writing the setting value to memory
//!
//! @param strSection Name of section
//! @param strSetting Name of setting
//! @param strValue New setting value
//------------------------------------------------------------------------
void CGridColumnConfigDefault::CGridColumnConfigLocal::WriteSetting(const CString& strSection, const CString& strSetting, const CString& strValue)
{
	m_LocalSettings.Add(strSetting, strValue);
}

//------------------------------------------------------------------------
//! Implements an interface for removing the setting section from memory
//!
//! @param strSection Name of section
//------------------------------------------------------------------------
void CGridColumnConfigDefault::CGridColumnConfigLocal::RemoveSection(const CString& strSection)
{
	m_LocalSettings.RemoveAll();
}

//------------------------------------------------------------------------
//! Contains default configuration
//!
//! @return Default configuration available (true/false)
//------------------------------------------------------------------------
bool CGridColumnConfigDefault::CGridColumnConfigLocal::HasSettings() const
{
	return m_LocalSettings.GetSize() > 0;
}

//------------------------------------------------------------------------
//! Copy the default values to another persistence layer
//!
//! @param destination The other persistence layer
//------------------------------------------------------------------------
void CGridColumnConfigDefault::CGridColumnConfigLocal::CopySettings(CGridColumnConfig& destination) const
{
	for(int i = 0; i < m_LocalSettings.GetSize(); ++i)
		destination.SetSetting(m_LocalSettings.GetKeyAt(i), m_LocalSettings.GetValueAt(i));
}

//------------------------------------------------------------------------
//! CGridColumnConfigDefault - Constructor
//!
//! @param strViewName Name to identify and persist the configuration
//------------------------------------------------------------------------
CGridColumnConfigDefault::CGridColumnConfigDefault(const CString& strViewName)
:CGridColumnConfig(strViewName)
,m_DefaultConfig(strViewName)
{}

//------------------------------------------------------------------------
//! Retrieves a setting value for the view. If the value is not available
//! then it returns the value from the default configuration.
//!
//! @param strName Name of setting
//! @param strDefval Default value to return if no value was found
//! @return Value of the setting
//------------------------------------------------------------------------
CString CGridColumnConfigDefault::GetSetting(const CString& strName, const CString& strDefval) const
{
	return CGridColumnConfig::GetSetting(strName, m_DefaultConfig.GetSetting(strName, strDefval));
}

//------------------------------------------------------------------------
//! Retrieve the in memory default configuration
//!
//! @return Default configuration
//------------------------------------------------------------------------
CGridColumnConfig& CGridColumnConfigDefault::GetDefaultConfig()
{
	return m_DefaultConfig;
}

//------------------------------------------------------------------------
//! Contains default configuration
//!
//! @return Default configuration available (true/false)
//------------------------------------------------------------------------
bool CGridColumnConfigDefault::HasDefaultConfig() const
{
	return m_DefaultConfig.HasSettings();
}

//------------------------------------------------------------------------
//! Resets the current configuration by deleting it and restoring it
//! from the in memory default configuration.
//------------------------------------------------------------------------
void CGridColumnConfigDefault::ResetConfigDefault()
{
	RemoveSection(GetSectionName());
	m_DefaultConfig.CopySettings(*this);
}

//------------------------------------------------------------------------
//! CGridColumnConfigProfiles - Constructor
//!
//! @param strViewName Name to identify and persist the configuration
//------------------------------------------------------------------------
CGridColumnConfigProfiles::CGridColumnConfigProfiles(const CString& strViewName)
:CGridColumnConfigDefault(strViewName)
{
	m_CurrentSection = strViewName;
}

//------------------------------------------------------------------------
//! Splits the section name into a view-name and profile-name
//!
//! @param strSection Name of the section
//! @param strViewName Name of the configuration
//! @param strProfile Name of a profile in the configuration
//------------------------------------------------------------------------
void CGridColumnConfigProfiles::SplitSectionName(const CString& strSection, CString& strViewName, CString& strProfile)
{
	int pos_profile = strSection.Find(_T("__"));
	if (pos_profile > 0)
	{
		strViewName = strSection.Mid(0, pos_profile);
		if (strViewName!=m_ViewName)
			strViewName = m_ViewName;
		else
			strProfile = strSection.Mid(pos_profile+2);
	}
	else
	{
		strViewName = m_ViewName;
	}
}

//------------------------------------------------------------------------
//! Combines the view-name and the profile-name into a section name
//!
//! @param strViewName Name of the configuration
//! @param strProfile Name of a profile in the configuration
//! @return The section name
//------------------------------------------------------------------------
CString CGridColumnConfigProfiles::JoinSectionName(const CString& strViewName, const CString& strProfile) const
{
	if (strProfile.IsEmpty())
		return strViewName;
	else
		return strViewName  + _T("__") + strProfile;
}

//------------------------------------------------------------------------
//! Retrieves the section name of the currently active configuration profile
//!
//! @return Current section name
//------------------------------------------------------------------------
const CString& CGridColumnConfigProfiles::GetSectionName() const
{
	if (m_CurrentSection==m_ViewName)
	{
		CString strProfile = ReadSetting(m_ViewName, _T("ActiveProfile"), _T(""));
		if (strProfile.IsEmpty())
		{
			CSimpleArray<CString> profiles;
			GetProfiles(profiles);
			if (profiles.GetSize()>0)
				strProfile = profiles[0];
		}
		m_CurrentSection = JoinSectionName(m_ViewName, strProfile);
	}
	return m_CurrentSection;
}

//------------------------------------------------------------------------
//! Retrieves the list of currently available configuration profiles
//!
//! @param profiles List of profile names
//------------------------------------------------------------------------
void CGridColumnConfigProfiles::GetProfiles(CSimpleArray<CString>& profiles) const
{
	const CString& strProfiles = ReadSetting(m_ViewName, _T("CurrentProfiles"), _T(""));
	SplitArraySetting(strProfiles, profiles, _T(", "));
}

//------------------------------------------------------------------------
//! Retrieves the currently active profile
//!
//! @return Name of the profile
//------------------------------------------------------------------------
CString CGridColumnConfigProfiles::GetActiveProfile()
{
	CString strViewName, strProfile;
	SplitSectionName(m_CurrentSection, strViewName, strProfile);
	return strProfile;
}

//------------------------------------------------------------------------
//! Switches to a new active configuration profile
//!
//! @param strProfile Name of the new profile
//------------------------------------------------------------------------
void CGridColumnConfigProfiles::SetActiveProfile(const CString& strProfile)
{
	// Make the new strProfile the active ones
	WriteSetting(m_ViewName, _T("ActiveProfile"), strProfile);
	m_CurrentSection = JoinSectionName(m_ViewName,strProfile);
	if (strProfile.IsEmpty())
		return;

	AddProfile(strProfile);
}

//------------------------------------------------------------------------
//! Adds a profile to the official list of column configuration profiles
//!
//! @param strProfile Name of the profile
//------------------------------------------------------------------------
void CGridColumnConfigProfiles::AddProfile(const CString& strProfile)
{
	// Add the strProfile to the list if not already there
	CSimpleArray<CString> profiles;
	GetProfiles(profiles);
	for(int i=0; i < profiles.GetSize(); ++i)
		if (profiles[i]==strProfile)
			return;

	CString noconst(strProfile);
	profiles.Add(noconst);

	WriteSetting(m_ViewName, _T("CurrentProfiles"), ConvertArraySetting(profiles, _T(", ")));
}

//------------------------------------------------------------------------
//! Removes a profile from the official list of column configuration profiles
//!
//! @param strProfile Name of the profile
//------------------------------------------------------------------------
void CGridColumnConfigProfiles::DeleteProfile(const CString& strProfile)
{
	if (strProfile.IsEmpty())
		return;

	// Remove any settings
	RemoveSection(JoinSectionName(m_ViewName,strProfile));

	// Remove the strProfile from the list
	CSimpleArray<CString> profiles;
	GetProfiles(profiles);
	for(int i=0; i < profiles.GetSize(); ++i)
		if (profiles[i]==strProfile)
			profiles.RemoveAt(i);
	WriteSetting(m_ViewName, _T("CurrentProfiles"), ConvertArraySetting(profiles, _T(", ")));
}

//------------------------------------------------------------------------
//! Removes the current configuration
//------------------------------------------------------------------------
void CGridColumnConfigProfiles::RemoveCurrentConfig()
{
	if (GetSectionName()==m_ViewName)
	{
		// Backup strProfile-settings and reset the other settings
		const CString& strProfiles = ReadSetting(m_ViewName, _T("CurrentProfiles"), _T(""));
		const CString& activeProfile = ReadSetting(m_ViewName, _T("ActiveProfile"), _T(""));
		CGridColumnConfigDefault::RemoveCurrentConfig();
		WriteSetting(m_ViewName, _T("CurrentProfiles"), strProfiles);
		WriteSetting(m_ViewName, _T("ActiveProfile"), activeProfile);
	}
	else
	{
		CGridColumnConfigDefault::RemoveCurrentConfig();
	}
}

//------------------------------------------------------------------------
//! CGridColumnConfigWinApp - Constructor
//!
//! @param strViewName Name to identify and persist the configuration
//------------------------------------------------------------------------
CGridColumnConfigWinApp::CGridColumnConfigWinApp(const CString& strViewName)
	:CGridColumnConfigProfiles(strViewName)
{
}

//------------------------------------------------------------------------
//! Implements an interface for reading the setting value CWinApp profile
//!
//! @param strSection Name of section
//! @param strSetting Name of setting
//! @param strDefval Default value to return if no value was found
//! @return Value of the setting
//------------------------------------------------------------------------
CString CGridColumnConfigWinApp::ReadSetting(const CString& strSection, const CString& strSetting, const CString& strDefval) const
{
	return AfxGetApp()->GetProfileString(strSection, strSetting, strDefval);
}

//------------------------------------------------------------------------
//! Implements an interface for writing the setting value to CWinApp profile
//!
//! @param strSection Name of section
//! @param strSetting Name of setting
//! @param strValue New setting value
//------------------------------------------------------------------------
void CGridColumnConfigWinApp::WriteSetting(const CString& strSection, const CString& strSetting, const CString& strValue)
{
	AfxGetApp()->WriteProfileString(strSection, strSetting, strValue);
}

//------------------------------------------------------------------------
//! Implements an interface for removing the setting section from memory
//!
//! @param strSection Name of section
//------------------------------------------------------------------------
void CGridColumnConfigWinApp::RemoveSection(const CString& strSection)
{
	// Section is deleted when providing NULL as entry
	AfxGetApp()->WriteProfileString(strSection, NULL, NULL);
}