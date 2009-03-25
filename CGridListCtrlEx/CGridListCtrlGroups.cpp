#include "stdafx.h"
#include "CGridListCtrlGroups.h"

#include <shlwapi.h>	// IsCommonControlsEnabled

#include "CGridColumnTrait.h"
#include "CGridColumnEditor.h"

// WIN32 defines for group-support is only available from 2003 PSDK
#if _WIN32_WINNT >= 0x0501

BEGIN_MESSAGE_MAP(CGridListCtrlGroups, CGridListCtrlEx)
#if _WIN32_WINNT >= 0x0600
	ON_NOTIFY_REFLECT_EX(LVN_LINKCLICK, OnGroupTaskClick)	// Task-Link Click
	ON_NOTIFY_REFLECT_EX(LVN_GETEMPTYMARKUP, OnGetEmptyMarkup)	// Request text to display when empty
#endif
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

//------------------------------------------------------------------------
//! Constructor 
//------------------------------------------------------------------------
CGridListCtrlGroups::CGridListCtrlGroups()
	:m_GroupHeight(-1)
{}

//------------------------------------------------------------------------
//! Inserts a group into the list view control.
//!
//! @param nIndex The insert position of the group
//! @param nGroupID ID of the new group
//! @param strHeader The group header title
//! @param dwState Specifies the state of the group when inserted
//! @param dwAlign Indicates the alignment of the header or footer text for the group
//! @return Returns the index of the item that the group was added to, or -1 if the operation failed.
//------------------------------------------------------------------------
LRESULT CGridListCtrlGroups::InsertGroupHeader(int nIndex, int nGroupID, const CString& strHeader, DWORD dwState /* = LVGS_NORMAL */, DWORD dwAlign /*= LVGA_HEADER_LEFT*/)
{
	LVGROUP lg = {0};
	lg.cbSize = sizeof(lg);
	lg.iGroupId = nGroupID;
	lg.state = dwState;
	lg.mask = LVGF_GROUPID | LVGF_HEADER | LVGF_STATE | LVGF_ALIGN;
	lg.uAlign = dwAlign;

	// Header-title must be unicode (Convert if necessary)
#ifdef UNICODE
	lg.pszHeader = (LPWSTR)(LPCTSTR)strHeader;
	lg.cchHeader = strHeader.GetLength();
#else
	CComBSTR header = strHeader;
	lg.pszHeader = header;
	lg.cchHeader = header.Length();
#endif

	return InsertGroup(nIndex, (PLVGROUP)&lg );
}

//------------------------------------------------------------------------
//! Moves a row into a group
//!
//! @param nRow The index of the row
//! @param nGroupID ID of the group
//! @return Nonzero if successful; otherwise zero
//------------------------------------------------------------------------
BOOL CGridListCtrlGroups::SetRowGroupId(int nRow, int nGroupID)
{
	//OBS! Rows not assigned to a group will not show in group-view
	LVITEM lvItem = {0};
	lvItem.mask = LVIF_GROUPID;
	lvItem.iItem = nRow;
	lvItem.iSubItem = 0;
	lvItem.iGroupId = nGroupID;
	return SetItem( &lvItem );
}

//------------------------------------------------------------------------
//! Retrieves the group id of a row
//!
//! @param nRow The index of the row
//! @return ID of the group
//------------------------------------------------------------------------
int CGridListCtrlGroups::GetRowGroupId(int nRow)
{
	LVITEM lvi = {0};
    lvi.mask = LVIF_GROUPID;
    lvi.iItem = nRow;
	VERIFY( GetItem(&lvi) );
    return lvi.iGroupId;
}

//------------------------------------------------------------------------
//! Retrieves the group header title of a group
//!
//! @param nGroupID ID of the group
//! @return Group header title
//------------------------------------------------------------------------
CString CGridListCtrlGroups::GetGroupHeader(int nGroupID)
{
	LVGROUP lg = {0};
	lg.cbSize = sizeof(lg);
	lg.iGroupId = nGroupID;
	lg.mask = LVGF_HEADER | LVGF_GROUPID;
	VERIFY( GetGroupInfo(nGroupID, (PLVGROUP)&lg) != -1 );

#ifdef UNICODE
	return lg.pszHeader;
#elif  _MSC_VER >= 1300
	CComBSTR header( lg.pszHeader );
	return (LPCTSTR)COLE2T(header);
#else
	USES_CONVERSION;
	return W2A(lg.pszHeader);
#endif
}

//------------------------------------------------------------------------
//! Checks is it is possible to modify the collapse state of a group.
//! This is only possible in Windows Vista.
//!
//! @return Groups can be collapsed (true / false)
//------------------------------------------------------------------------
BOOL CGridListCtrlGroups::IsGroupStateEnabled()
{
	if (!IsGroupViewEnabled())
		return FALSE;

	OSVERSIONINFO osver = {0};
	osver.dwOSVersionInfoSize = sizeof(osver);
	GetVersionEx(&osver);
	WORD fullver = MAKEWORD(osver.dwMinorVersion, osver.dwMajorVersion);
	if (fullver < 0x0600)
		return FALSE;

	return TRUE;
}

//------------------------------------------------------------------------
//! Checks whether a group has a certain state
//!
//! @param nGroupID ID of the group
//! @param dwState Specifies the state to check
//! @return The group has the state (true / false)
//------------------------------------------------------------------------
BOOL CGridListCtrlGroups::HasGroupState(int nGroupID, DWORD dwState)
{
	// Vista SDK - ListView_GetGroupState / LVM_GETGROUPSTATE
	LVGROUP lg = {0};
	lg.cbSize = sizeof(lg);
	lg.mask = LVGF_STATE;
	lg.stateMask = dwState;
	if ( GetGroupInfo(nGroupID, (PLVGROUP)&lg) == -1)
		return FALSE;

	return lg.state==dwState;
}

//------------------------------------------------------------------------
//! Updates the state of a group
//!
//! @param nGroupID ID of the group
//! @param dwState Specifies the new state of the group
//! @return The group state was updated (true / false)
//------------------------------------------------------------------------
BOOL CGridListCtrlGroups::SetGroupState(int nGroupID, DWORD dwState)
{
	// Vista SDK - ListView_SetGroupState / LVM_SETGROUPINFO
	if (!IsGroupStateEnabled())
		return FALSE;

	LVGROUP lg = {0};
	lg.cbSize = sizeof(lg);
	lg.mask = LVGF_STATE;
	lg.state = dwState;
	lg.stateMask = dwState;

#ifdef LVGS_COLLAPSIBLE
	// Maintain LVGS_COLLAPSIBLE state
	if (HasGroupState(nGroupID, LVGS_COLLAPSIBLE))
		lg.state |= LVGS_COLLAPSIBLE;
#endif

	if (SetGroupInfo(nGroupID, (PLVGROUP)&lg)==-1)
		return FALSE;

	return TRUE;
}

//------------------------------------------------------------------------
//! Find the group-id below the given point
//!
//! @param point Mouse position
//! @return ID of the group
//------------------------------------------------------------------------
int CGridListCtrlGroups::GroupHitTest(const CPoint& point)
{
	if (!IsGroupViewEnabled())
		return -1;

	if (HitTest(point)!=-1)
		return -1;

	if (IsGroupStateEnabled())
	{
#if _WIN32_WINNT >= 0x0600
#ifdef ListView_HitTestEx
#ifdef LVHT_EX_GROUP
		LVHITTESTINFO lvhitinfo = {0};
		lvhitinfo.pt = point;
		ListView_HitTestEx(m_hWnd, &lvhitinfo);
		if ((lvhitinfo.flags & LVHT_EX_GROUP)==0)
			return -1;
#endif
#endif
#endif

#if _WIN32_WINNT >= 0x0600
#ifdef ListView_GetGroupCount
#ifdef ListView_GetGroupRect
#ifdef ListView_GetGroupInfoByIndex
		LRESULT groupCount = ListView_GetGroupCount(m_hWnd);
		if (groupCount <= 0)
			return -1;
		for(int i = 0 ; i < groupCount; ++i)
		{
			LVGROUP lg = {0};
			lg.cbSize = sizeof(lg);
			lg.mask = LVGF_GROUPID;
			VERIFY( ListView_GetGroupInfoByIndex(m_hWnd, i, &lg) );

			CRect rect(0,0,0,0);
			VERIFY( ListView_GetGroupRect(m_hWnd, lg.iGroupId, 0, &rect) );

			if (rect.PtInRect(point))
				return lg.iGroupId;
		}
		// Don't try other ways to find the group
		if (groupCount > 0)
			return -1;
#endif
#endif
#endif
#endif
	}	// IsGroupStateEnabled()

	CRect headerRect;
	GetHeaderCtrl()->GetClientRect(&headerRect);
	if (headerRect.PtInRect(point))
		return -1;

	// We require that each group contains atleast one item
	if (GetItemCount()==0)
		return -1;

	CRect gridRect(0,0,0,0);
	GetClientRect(&gridRect);

	// Loop through all rows until a visible row below the point is found
	int nPrevGroupId = -1, nCollapsedGroups = 0;
	int nRowAbove = -1, nRowBelow = 0;
	for(nRowBelow = 0; nRowBelow < GetItemCount(); nRowBelow++)
	{
		int nGroupId = GetRowGroupId(nRowBelow);

		CRect rectRowBelow;
		if (GetItemRect(nRowBelow, rectRowBelow, LVIR_BOUNDS)==FALSE)
		{
			// Found invisible row (row within collapsed group)
			if (nGroupId != -1 && nGroupId != nPrevGroupId)
			{
				//	Test if the point is within a collapsed group
				nCollapsedGroups++;
				nPrevGroupId = nGroupId;
				CRect groupRect = gridRect;
				if (nRowAbove!=-1)
				{
					GetItemRect(nRowAbove, groupRect, LVIR_BOUNDS);
					groupRect.right = gridRect.right;
				}
				else
				{
					groupRect.bottom = headerRect.bottom;
				}
				groupRect.bottom += m_GroupHeight * nCollapsedGroups;
				if (groupRect.PtInRect(point))
					return nGroupId;	// Hit a collapsed group
			}
			continue;	
		}

		// Cheating by remembering the group-height from the first visible row
		if (m_GroupHeight==-1)
		{
			m_GroupHeight = rectRowBelow.top - headerRect.Height();
		}

		rectRowBelow.right = gridRect.right;
		if (rectRowBelow.PtInRect(point))
			return -1;	// Hit a row
		if (rectRowBelow.top > point.y)
			break;		// Found row just below the point

		nCollapsedGroups = 0;
		nRowAbove = nRowBelow;
	}

	if (nRowBelow < GetItemCount())
	{
		// Probably hit a group above this row
		return GetRowGroupId(nRowBelow);
	}

	return -1;
}

//------------------------------------------------------------------------
//! Update the checkbox of the label column (first column)
//!
//! @param nGroupID ID of the group
//! @param bChecked The new check box state
//------------------------------------------------------------------------
void CGridListCtrlGroups::CheckEntireGroup(int nGroupId, bool bChecked)
{
	if (!(GetExtendedStyle() & LVS_EX_CHECKBOXES))
		return;

	for (int nRow=0; nRow<GetItemCount(); ++nRow)
	{
		if (GetRowGroupId(nRow) == nGroupId)
		{
			SetCheck(nRow, bChecked ? TRUE : FALSE);
		}
	}
}

//------------------------------------------------------------------------
//! Removes the group and all the rows part of the group
//!
//! @param nGroupID ID of the group
//------------------------------------------------------------------------
void CGridListCtrlGroups::DeleteEntireGroup(int nGroupId)
{
	for (int nRow=0; nRow<GetItemCount(); ++nRow)
	{
		if (GetRowGroupId(nRow) == nGroupId)
		{
			DeleteItem(nRow);
			nRow--;
		}
	}
	RemoveGroup(nGroupId);
}

//------------------------------------------------------------------------
//! Create a group for each unique values within a column
//!
//! @param nCol The index of the column
//! @return Succeeded in creating the group
//------------------------------------------------------------------------
BOOL CGridListCtrlGroups::GroupByColumn(int nCol)
{
	SetSortArrow(-1, false);

	RemoveAllGroups();

	EnableGroupView( GetItemCount() > 0 );

	if (IsGroupViewEnabled())
	{
		CSimpleMap<CString,CSimpleArray<int> > groups;

		// Loop through all rows and find possible groups
		for(int nRow=0; nRow<GetItemCount(); ++nRow)
		{
			CString cellText = GetItemText(nRow, nCol);

			int nGroupId = groups.FindKey(cellText);
			if (nGroupId==-1)
			{
				CSimpleArray<int> rows;
				groups.Add(cellText, rows);
				nGroupId = groups.FindKey(cellText);
			}
			groups.GetValueAt(nGroupId).Add(nRow);
		}

		// Look through all groups and assign rows to group
		for(int nGroupId = 0; nGroupId < groups.GetSize(); ++nGroupId)
		{
			const CSimpleArray<int>& groupRows = groups.GetValueAt(nGroupId);
			DWORD dwState = LVGS_NORMAL;
#ifdef LVGS_COLLAPSIBLE
			if (IsGroupStateEnabled())
				dwState = LVGS_COLLAPSIBLE;
#endif
			VERIFY( InsertGroupHeader(nGroupId, nGroupId, groups.GetKeyAt(nGroupId), dwState) != -1);

			for(int groupRow = 0; groupRow < groupRows.GetSize(); ++groupRow)
			{
				VERIFY( SetRowGroupId(groupRows[groupRow], nGroupId) );
			}
		}
		return TRUE;
	}

	return FALSE;
}

//------------------------------------------------------------------------
//! Collapse all groups
//------------------------------------------------------------------------
void CGridListCtrlGroups::CollapseAllGroups()
{
	// Loop through all rows and find possible groups
	for(int nRow=0; nRow<GetItemCount(); ++nRow)
	{
		int nGroupId = GetRowGroupId(nRow);
		if (nGroupId!=-1)
		{
			if (!HasGroupState(nGroupId,LVGS_COLLAPSED))
			{
				SetGroupState(nGroupId,LVGS_COLLAPSED);
			}
		}
	}
}

//------------------------------------------------------------------------
//! Expand all groups
//------------------------------------------------------------------------
void CGridListCtrlGroups::ExpandAllGroups()
{
	// Loop through all rows and find possible groups
	for(int nRow=0; nRow<GetItemCount(); ++nRow)
	{
		int nGroupId = GetRowGroupId(nRow);
		if (nGroupId!=-1)
		{
			if (HasGroupState(nGroupId,LVGS_COLLAPSED))
			{
				SetGroupState(nGroupId,LVGS_NORMAL);
			}
		}
	}
}

//------------------------------------------------------------------------
//! WM_CONTEXTMENU message handler to show popup menu when mouse right
//! click is used (or SHIFT+F10 on the keyboard)
//!
//! @param pWnd Handle to the window in which the user right clicked the mouse
//! @param point Position of the cursor, in screen coordinates, at the time of the mouse click.
//------------------------------------------------------------------------
void CGridListCtrlGroups::OnContextMenu(CWnd* pWnd, CPoint point)
{
	if ( IsGroupViewEnabled() )
	{
		if (point.x!=-1 && point.y!=-1)
		{
			CPoint pt = point;
			ScreenToClient(&pt);

			int nGroupId = GroupHitTest(pt);
			if (nGroupId!=-1)
			{
				OnContextMenuGroup(pWnd, point, nGroupId);
				return;
			}
		}
	}
	CGridListCtrlEx::OnContextMenu(pWnd, point);
}

namespace {
	bool IsCommonControlsEnabled()
	{
		// Test if application has access to common controls (Required for grouping)
		HMODULE hinstDll = ::LoadLibrary(_T("comctl32.dll"));
		if (hinstDll)
		{
			DLLGETVERSIONPROC pDllGetVersion = (DLLGETVERSIONPROC)::GetProcAddress(hinstDll, "DllGetVersion");
			::FreeLibrary(hinstDll);
			if (pDllGetVersion != NULL)
			{
				DLLVERSIONINFO dvi = {0};
				dvi.cbSize = sizeof(dvi);
				HRESULT hRes = pDllGetVersion ((DLLVERSIONINFO *) &dvi);
				if (SUCCEEDED(hRes))
					return dvi.dwMajorVersion >= 6;
			}
		}
		return false;
	}
}

//------------------------------------------------------------------------
//! Override this method to change the context menu when activating context
//! menu for the column headers
//!
//! @param pWnd Handle to the window in which the user right clicked the mouse
//! @param point Position of the cursor, in screen coordinates, at the time of the mouse click.
//! @param nCol The index of the column
//------------------------------------------------------------------------
void CGridListCtrlGroups::OnContextMenuHeader(CWnd* pWnd, CPoint point, int nCol)
{
	if (!IsCommonControlsEnabled())
	{
		CGridListCtrlEx::OnContextMenuHeader(pWnd, point, nCol);
		return;
	}

	// Show context-menu with the option to show hide columns
	CMenu menu;
	VERIFY( menu.CreatePopupMenu() );

	if (nCol!=-1)
	{
		// Retrieve column-title
		const CString& columnTitle = GetColumnHeading(nCol);
		menu.AppendMenu(MF_STRING, 3, CString(_T("Group by: ")) + columnTitle);
	}

	if (IsGroupViewEnabled())
	{
		menu.AppendMenu(MF_STRING, 4, _T("Disable grouping"));
	}

	CString title_editor;
	if (m_pColumnEditor->HasColumnEditor(*this, nCol, title_editor))
	{
		menu.AppendMenu(MF_STRING, 1, static_cast<LPCTSTR>(title_editor));
	}

	CString title_picker;
	if (m_pColumnEditor->HasColumnPicker(*this, title_picker))
	{
		menu.AppendMenu(MF_STRING, 2, static_cast<LPCTSTR>(title_picker));		
	}
	else
	{
		if (menu.GetMenuItemCount()>0)
			menu.AppendMenu(MF_SEPARATOR, 0, _T(""));

		InternalColumnPicker(menu, 6);
	}

	CSimpleArray<CString> profiles;
	InternalColumnProfileSwitcher(menu, GetColumnCount() + 7, profiles);

	CString title_resetdefault;
	if (m_pColumnEditor->HasColumnsDefault(*this, title_resetdefault))
	{
		if (profiles.GetSize()==0)
			menu.AppendMenu(MF_SEPARATOR, 0, _T(""));
		menu.AppendMenu(MF_STRING, 5, title_resetdefault);
	}

	// Will return zero if no selection was made (TPM_RETURNCMD)
	int nResult = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RETURNCMD, point.x, point.y, this, 0);
	switch(nResult)
	{
		case 0: break;
		case 1:	m_pColumnEditor->OpenColumnEditor(*this, nCol); break;
		case 2: m_pColumnEditor->OpenColumnPicker(*this); break;
		case 3: GroupByColumn(nCol); break;
		case 4: RemoveAllGroups(); EnableGroupView(FALSE); break;
		case 5: m_pColumnEditor->ResetColumnsDefault(*this); break;
		default:
		{
			int nCol = nResult-6;
			if (nCol < GetColumnCount())
			{
				ShowColumn(nCol, !IsColumnVisible(nCol));
			}
			else
			{
				int nProfile = nResult-GetColumnCount()-7;
				m_pColumnEditor->SwichColumnProfile(*this, profiles[nProfile]);
			}
		} break;
	}
}

//------------------------------------------------------------------------
//! Override this method to change the context menu when activating context
//! menu for the group headers
//!
//! @param pWnd Handle to the window in which the user right clicked the mouse
//! @param point Position of the cursor, in screen coordinates, at the time of the mouse click.
//! @param nGroupID ID of the group
//------------------------------------------------------------------------
void CGridListCtrlGroups::OnContextMenuGroup(CWnd* pWnd, CPoint point, int nGroupId)
{
	CMenu menu;
	UINT uFlags = MF_BYPOSITION | MF_STRING;
	VERIFY( menu.CreatePopupMenu() );
	
	const CString& groupHeader = GetGroupHeader(nGroupId);

#ifndef LVGS_COLLAPSIBLE
	if (IsGroupStateEnabled())
	{
		if (HasGroupState(nGroupId,LVGS_COLLAPSED))
		{
			CString menuText = CString(_T("Expand group: ")) + groupHeader;
			menu.InsertMenu(0, uFlags, 1, menuText);
		}
		else
		{
			CString menuText = CString(_T("Collapse group: ")) + groupHeader;
			menu.InsertMenu(0, uFlags, 2, menuText);
		}
	}
#endif
	if (GetExtendedStyle() & LVS_EX_CHECKBOXES)
	{
		CString menuText = CString(_T("Check group: ")) + groupHeader;
		menu.InsertMenu(1, uFlags, 3, menuText);
		menuText = CString(_T("Uncheck group: ")) + groupHeader;
		menu.InsertMenu(2, uFlags, 4, menuText);
	}

	int nResult = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RETURNCMD, point.x, point.y, this, 0);
	switch(nResult)
	{
		case 1: SetGroupState(nGroupId,LVGS_NORMAL); break;
		case 2: SetGroupState(nGroupId,LVGS_COLLAPSED); break;
		case 3: CheckEntireGroup(nGroupId, true); break;
		case 4: CheckEntireGroup(nGroupId, false); break;
	}
}

//------------------------------------------------------------------------
//! Override this method to change the context menu when activating context
//! menu in client area with no rows
//!
//! @param pWnd Handle to the window in which the user right clicked the mouse
//! @param point Position of the cursor, in screen coordinates, at the time of the mouse click.
//------------------------------------------------------------------------
void CGridListCtrlGroups::OnContextMenuGrid(CWnd* pWnd, CPoint point)
{
	if (IsGroupStateEnabled())
	{
		CMenu menu;
		UINT uFlags = MF_BYPOSITION | MF_STRING;
		VERIFY( menu.CreatePopupMenu() );

		menu.InsertMenu(0, uFlags, 1, _T("Expand all groups"));
		menu.InsertMenu(1, uFlags, 2, _T("Collapse all groups"));
		menu.InsertMenu(2, uFlags, 3, _T("Disable grouping"));

		int nResult = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RETURNCMD, point.x, point.y, this, 0);
		switch(nResult)
		{
			case 1: ExpandAllGroups(); break;
			case 2: CollapseAllGroups(); break;
			case 3: RemoveAllGroups(); EnableGroupView(FALSE); break;
		}
	}
}

//------------------------------------------------------------------------
//! Update the description text of the group footer
//!
//! @param nGroupID ID of the group
//! @param strFooter The footer description text
//! @param dwAlign Indicates the alignment of the footer text for the group
//! @return Succeeded in updating the group footer
//------------------------------------------------------------------------
BOOL CGridListCtrlGroups::SetGroupFooter(int nGroupID, const CString& strFooter, DWORD dwAlign)
{
	if (!IsGroupStateEnabled())
		return FALSE;

#if _WIN32_WINNT >= 0x0600
	LVGROUP lg = {0};
	lg.cbSize = sizeof(lg);
	lg.mask = LVGF_FOOTER | LVGF_ALIGN;
	lg.uAlign = dwAlign;
#ifdef UNICODE
	lg.pszFooter = (LPWSTR)(LPCTSTR)strFooter;
	lg.cchFooter = strFooter.GetLength();
#else
	CComBSTR bstrFooter = strFooter;
	lg.pszFooter = bstrFooter;
	lg.cchFooter = bstrFooter.Length();
#endif

	if (SetGroupInfo(nGroupID, (PLVGROUP)&lg)==-1)
		return FALSE;

	return TRUE;
#else
	return FALSE;
#endif
}

//------------------------------------------------------------------------
//! Update the task link of the group header
//!
//! @param nGroupID ID of the group
//! @param strTask The task description text
//! @return Succeeded in updating the group task
//------------------------------------------------------------------------
BOOL CGridListCtrlGroups::SetGroupTask(int nGroupID, const CString& strTask)
{
	if (!IsGroupStateEnabled())
		return FALSE;

#if _WIN32_WINNT >= 0x0600
	LVGROUP lg = {0};
	lg.cbSize = sizeof(lg);
	lg.mask = LVGF_TASK;
#ifdef UNICODE
	lg.pszTask = (LPWSTR)(LPCTSTR)strTask;
	lg.cchTask = strTask.GetLength();
#else
	CComBSTR bstrTask = strTask;
	lg.pszTask = bstrTask;
	lg.cchTask = bstrTask.Length();
#endif

	if (SetGroupInfo(nGroupID, (PLVGROUP)&lg)==-1)
		return FALSE;

	return TRUE;
#else
	return FALSE;
#endif
}

//------------------------------------------------------------------------
//! Update the subtitle in the group header
//!
//! @param nGroupID ID of the group
//! @param strSubtitle The subtitle description text
//! @return Succeeded in updating the group subtitle
//------------------------------------------------------------------------
BOOL CGridListCtrlGroups::SetGroupSubtitle(int nGroupID, const CString& strSubtitle)
{
	if (!IsGroupStateEnabled())
		return FALSE;

#if _WIN32_WINNT >= 0x0600
	LVGROUP lg = {0};
	lg.cbSize = sizeof(lg);
	lg.mask = LVGF_SUBTITLE;
#ifdef UNICODE
	lg.pszSubtitle = (LPWSTR)(LPCTSTR)strSubtitle;
	lg.cchSubtitle = strSubtitle.GetLength();
#else
	CComBSTR bstrSubtitle = strSubtitle;
	lg.pszSubtitle = bstrSubtitle;
	lg.cchSubtitle = bstrSubtitle.Length();
#endif

	if (SetGroupInfo(nGroupID, (PLVGROUP)&lg)==-1)
		return FALSE;

	return TRUE;
#else
	return FALSE;
#endif
}

//------------------------------------------------------------------------
//! Update the image icon in the group header together with top and bottom
//! description. Microsoft encourage people not to use this functionality.
//!
//! @param nGroupID ID of the group
//! @param nImage Index of the title image in the control imagelist.
//! @param strTopDesc Description text placed oppposite of the image
//! @param strBottomDesc Description text placed below the top description
//! @return Succeeded in updating the group image
//------------------------------------------------------------------------
BOOL CGridListCtrlGroups::SetGroupTitleImage(int nGroupID, int nImage, const CString& strTopDesc, const CString& strBottomDesc)
{
	if (!IsGroupStateEnabled())
		return FALSE;

#if _WIN32_WINNT >= 0x0600
	LVGROUP lg = {0};
	lg.cbSize = sizeof(lg);
	lg.mask = LVGF_TITLEIMAGE;
	lg.iTitleImage = nImage;	// Index of the title image in the control imagelist.

#ifdef UNICODE
	if (!strTopDesc.IsEmpty())
	{
		// Top description is drawn opposite the title image when there is
		// a title image, no extended image, and uAlign==LVGA_HEADER_CENTER.
		lg.mask |= LVGF_DESCRIPTIONTOP;
		lg.pszDescriptionTop = (LPWSTR)(LPCTSTR)strTopDesc;
		lg.cchDescriptionTop = strTopDesc.GetLength();
	}
	if (!strBottomDesc.IsEmpty())
	{
		// Bottom description is drawn under the top description text when there is
		// a title image, no extended image, and uAlign==LVGA_HEADER_CENTER.
		lg.mask |= LVGF_DESCRIPTIONBOTTOM;
		lg.pszDescriptionBottom = (LPWSTR)(LPCTSTR)strBottomDesc;
		lg.cchDescriptionBottom = strBottomDesc.GetLength();
	}
#else
	CComBSTR bstrTopDesc = strTopDesc;
	CComBSTR bstrBottomDesc = strBottomDesc;
	if (!strTopDesc.IsEmpty())
	{
		lg.mask |= LVGF_DESCRIPTIONTOP;
		lg.pszDescriptionTop = bstrTopDesc;
		lg.cchDescriptionTop = bstrTopDesc.Length();
	}
	if (!strBottomDesc.IsEmpty())
	{
		lg.mask |= LVGF_DESCRIPTIONBOTTOM;
		lg.pszDescriptionBottom = bstrBottomDesc;
		lg.cchDescriptionBottom = bstrBottomDesc.Length();
	}
#endif

	if (SetGroupInfo(nGroupID, (PLVGROUP)&lg)==-1)
		return FALSE;

	return TRUE;
#else
	return FALSE;
#endif
}

//------------------------------------------------------------------------
//! LVN_GETEMPTYMARKUP message handler to show markup text when the list
//! control is empty.
//!
//! @param pNMHDR Pointer to NMLVEMPTYMARKUP structure
//! @param pResult Not used
//! @return Is final message handler (Return FALSE to continue routing the message)
//------------------------------------------------------------------------
BOOL CGridListCtrlGroups::OnGetEmptyMarkup(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_EmptyMarkupText.IsEmpty())
		return FALSE;

#if _WIN32_WINNT >= 0x0600
	NMLVEMPTYMARKUP* pEmptyMarkup = reinterpret_cast<NMLVEMPTYMARKUP*>(pNMHDR);
	pEmptyMarkup->dwFlags = EMF_CENTERED;

#ifdef UNICODE
	lstrcpyn(pEmptyMarkup->szMarkup, (LPCTSTR)m_EmptyMarkupText, sizeof(pEmptyMarkup->szMarkup)/sizeof(WCHAR));
#else
#if __STDC_WANT_SECURE_LIB__
	mbstowcs_s(NULL, pEmptyMarkup->szMarkup, static_cast<LPCTSTR>(m_EmptyMarkupText), sizeof(pEmptyMarkup->szMarkup)/sizeof(WCHAR));
#else
	mbstowcs(pEmptyMarkup->szMarkup, static_cast<LPCTSTR>(m_EmptyMarkupText), sizeof(pEmptyMarkup->szMarkup)/sizeof(WCHAR));
#endif
#endif
	*pResult = TRUE;
#endif

	return TRUE;
}

//------------------------------------------------------------------------
//! LVN_LINKCLICK message handler called when a group task link is clicked
//!
//! @param pNMHDR Pointer to NMLVLINK structure
//! @param pResult Not used
//! @return Is final message handler (Return FALSE to continue routing the message)
//------------------------------------------------------------------------
BOOL CGridListCtrlGroups::OnGroupTaskClick(NMHDR* pNMHDR, LRESULT* pResult)
{
#if _WIN32_WINNT >= 0x0600
	NMLVLINK* pLinkInfo = reinterpret_cast<NMLVLINK*>(pNMHDR);
	int nGroupId = pLinkInfo->iSubItem;
	nGroupId;	// Avoid unreferenced variable warning
#endif
	return FALSE;
}

//------------------------------------------------------------------------
//! The framework calls this member function when the user double-clicks
//! the left mouse button. Used to expand and collapse groups when double
//! double click is used
//!
//! @param nFlags Indicates whether various virtual keys are down (MK_CONTROL, MK_SHIFT, etc.)
//! @param point Specifies the x- and y-coordinate of the cursor relative to the upper-left corner of the window.
//------------------------------------------------------------------------
void CGridListCtrlGroups::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	CGridListCtrlEx::OnLButtonDblClk(nFlags, point);
	int nGroupId = GroupHitTest(point);
	if (nGroupId!=-1)
	{
		if (HasGroupState(nGroupId, LVGS_COLLAPSED))
			SetGroupState(nGroupId, LVGS_NORMAL);
		else
			SetGroupState(nGroupId, LVGS_COLLAPSED);
	}
}

//------------------------------------------------------------------------
//! Override this method to provide the group a cell belongs to
//!
//! @param nRow The index of the row
//! @param nCol The index of the column
//! @param nGroupId Text string to display in the cell
//! @return True if the cell belongs to a group
//------------------------------------------------------------------------
bool CGridListCtrlGroups::OnDisplayCellGroup(int nRow, int nCol, int& nGroupId)
{
	return false;
}

//------------------------------------------------------------------------
//! Handles the LVN_GETDISPINFO message, which is sent when details are
//! needed for an item that specifies callback.
//!		- Cell-Group, when item is using I_GROUPIDCALLBACK
//!
//! @param pNMHDR Pointer to an NMLVDISPINFO structure
//! @param pResult Not used
//! @return Is final message handler (Return FALSE to continue routing the message)
//------------------------------------------------------------------------
BOOL CGridListCtrlGroups::OnGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVDISPINFO* pNMW = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	int nRow = pNMW->item.iItem;
	int nCol = pNMW->item.iSubItem;

	if (nRow< 0 || nRow >= GetItemCount())
		return FALSE;	// requesting invalid item

	if (nCol < 0 || nCol >= GetHeaderCtrl()->GetItemCount())
		return FALSE;	// requesting invalid item

	if (pNMW->item.mask & LVIF_GROUPID)
	{
		// Request group-id of the column (Virtual-list/LVS_OWNERDATA)
		int result = -1;
		if (OnDisplayCellGroup(nRow, nCol, result))
			pNMW->item.iGroupId = result;
		else
			pNMW->item.iGroupId = I_GROUPIDNONE;
	}
	return CGridListCtrlEx::OnGetDispInfo(pNMHDR, pResult);
}

namespace {
	struct PARAMSORT
	{
		PARAMSORT(HWND hWnd, int columnIndex, bool ascending)
			:m_hWnd(hWnd)
			,m_ColumnIndex(columnIndex)
			,m_Ascending(ascending)
		{}

		HWND m_hWnd;
		int  m_ColumnIndex;
		bool m_Ascending;
		CSimpleMap<int,CString> m_GroupNames;

		const CString& LookupGroupName(int nGroupId)
		{
			int groupIdx = m_GroupNames.FindKey(nGroupId);
			if (groupIdx==-1)
			{
				static const CString emptyStr;
				return emptyStr;
			}			
			return m_GroupNames.GetValueAt(groupIdx);
		}
	};

	int CALLBACK SortFuncGroup(int leftId, int rightId, void* lParamSort)
	{
		PARAMSORT& ps = *(PARAMSORT*)lParamSort;

		const CString& left = ps.LookupGroupName(leftId);
		const CString& right = ps.LookupGroupName(rightId);

		if (ps.m_Ascending)
			return _tcscmp( left, right );
		else
			return _tcscmp( right, left );
	}
}

//------------------------------------------------------------------------
//! Changes the row sorting in regard to the specified column
//!
//! @param nCol The index of the column
//! @param bAscending Should the arrow be up or down 
//! @return True / false depending on whether sort is possible
//------------------------------------------------------------------------
bool CGridListCtrlGroups::SortColumn(int nCol, bool bAscending)
{
	if (IsGroupViewEnabled())
	{
		PARAMSORT paramsort(m_hWnd, nCol, bAscending);

		GroupByColumn(nCol);

		// Cannot use GetGroupInfo during sort
		for(int nRow=0 ; nRow < GetItemCount() ; ++nRow)
		{
			int nGroupId = GetRowGroupId(nRow);
			if (nGroupId!=-1 && paramsort.m_GroupNames.FindKey(nGroupId)==-1)
				paramsort.m_GroupNames.Add(nGroupId, GetGroupHeader(nGroupId));
		}

		// Avoid bug in CListCtrl::SortGroups() which differs from ListView_SortGroups
		ListView_SortGroups(m_hWnd, SortFuncGroup, &paramsort);
	}

	// Always sort the rows, so the handicapped GroupHitTest() will work
	//	- Must ensure that the rows are reordered along with the groups.
	return CGridListCtrlEx::SortColumn(nCol, bAscending);
}

//------------------------------------------------------------------------
//! WM_PAINT message handler called when needing to redraw list control.
//! Used to display text when the list control is empty
//------------------------------------------------------------------------
void CGridListCtrlGroups::OnPaint()
{
#if _WIN32_WINNT >= 0x0600
	if (UsingVisualStyle())
	{
		// Use LVN_GETEMPTYMARKUP if available
		CListCtrl::OnPaint();	// default
		return;
	}
#endif

    CGridListCtrlEx::OnPaint();
}

// MFC headers with group-support is only availabe from VS.NET 
#if _MSC_VER < 1300

AFX_INLINE LRESULT CGridListCtrlGroups::InsertGroup(int index, PLVGROUP pgrp)
{
	ASSERT(::IsWindow(m_hWnd));
	return ListView_InsertGroup(m_hWnd, index, pgrp);
}
AFX_INLINE int CGridListCtrlGroups::SetGroupInfo(int iGroupId, PLVGROUP pgrp)
{
	ASSERT(::IsWindow(m_hWnd));
	return (int)ListView_SetGroupInfo(m_hWnd, iGroupId, pgrp);
}
AFX_INLINE int CGridListCtrlGroups::GetGroupInfo(int iGroupId, PLVGROUP pgrp) const
{
	ASSERT(::IsWindow(m_hWnd));
	return (int)ListView_GetGroupInfo(m_hWnd, iGroupId, pgrp);
}
AFX_INLINE LRESULT CGridListCtrlGroups::RemoveGroup(int iGroupId)
{
	ASSERT(::IsWindow(m_hWnd));
	return ListView_RemoveGroup(m_hWnd, iGroupId);
}
AFX_INLINE LRESULT CGridListCtrlGroups::MoveGroup(int iGroupId, int toIndex)
{
	ASSERT(::IsWindow(m_hWnd));
	return ListView_MoveGroup(m_hWnd, iGroupId, toIndex);
}
AFX_INLINE LRESULT CGridListCtrlGroups::MoveItemToGroup(int idItemFrom, int idGroupTo)
{
	ASSERT(::IsWindow(m_hWnd));
	return ListView_MoveItemToGroup(m_hWnd, idItemFrom, idGroupTo);
}
AFX_INLINE void CGridListCtrlGroups::SetGroupMetrics(PLVGROUPMETRICS pGroupMetrics)
{
	ASSERT(::IsWindow(m_hWnd));
	ListView_SetGroupMetrics(m_hWnd, pGroupMetrics);
}
AFX_INLINE void CGridListCtrlGroups::GetGroupMetrics(PLVGROUPMETRICS pGroupMetrics) const
{
	ASSERT(::IsWindow(m_hWnd));
	ListView_GetGroupMetrics(m_hWnd, pGroupMetrics);
}
AFX_INLINE LRESULT CGridListCtrlGroups::EnableGroupView(BOOL fEnable)
{
	ASSERT(::IsWindow(m_hWnd));
	return ListView_EnableGroupView(m_hWnd, fEnable);
}
AFX_INLINE BOOL CGridListCtrlGroups::SortGroups(PFNLVGROUPCOMPARE _pfnGroupCompare, LPVOID _plv)
{
	ASSERT(::IsWindow(m_hWnd));
	return (BOOL)::SendMessage(m_hWnd, LVM_SORTGROUPS, (WPARAM)(LPARAM)_plv, (LPARAM)_pfnGroupCompare );
}
AFX_INLINE LRESULT CGridListCtrlGroups::InsertGroupSorted(PLVINSERTGROUPSORTED pStructInsert)
{
	ASSERT(::IsWindow(m_hWnd));
	return ListView_InsertGroupSorted(m_hWnd, pStructInsert);
}
AFX_INLINE void CGridListCtrlGroups::RemoveAllGroups()
{
	ASSERT(::IsWindow(m_hWnd));
	ListView_RemoveAllGroups(m_hWnd);
}
AFX_INLINE BOOL CGridListCtrlGroups::HasGroup(int iGroupId) const
{
	ASSERT(::IsWindow(m_hWnd));
	return (BOOL)ListView_HasGroup(m_hWnd, iGroupId);
}
AFX_INLINE BOOL CGridListCtrlGroups::IsGroupViewEnabled() const
{
	ASSERT(::IsWindow(m_hWnd));
	return ListView_IsGroupViewEnabled(m_hWnd);
}
#endif // _MSC_VER < 1300

#endif // _WIN32_WINNT >= 0x0501
