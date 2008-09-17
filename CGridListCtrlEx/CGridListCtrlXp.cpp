#include "stdafx.h"
#include "CGridListCtrlXp.h"

// Redraw icon-rect with proper selection color
void CGridListCtrlXp::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (UsingVisualStyle())
	{
		CGridListCtrlGroups::OnCustomDraw(pNMHDR, pResult);
		return;
	}

	// We are using classic- or XP-style
	NMLVCUSTOMDRAW* pLVCD = (NMLVCUSTOMDRAW*)(pNMHDR);

	int nRow = (int)pLVCD->nmcd.dwItemSpec;

	switch (pLVCD->nmcd.dwDrawStage)
	{
		case CDDS_ITEMPOSTPAINT | CDDS_SUBITEM:
		{
			// Fix CListCtrl selection drawing bug with white background for icon image
			// Fix CListCtrl selection drawing bug with white margin between icon and text
			int nCol = pLVCD->iSubItem;

			if (!IsColumnVisible(nCol))
				break;

			if (!IsRowSelected(nRow))
				break;	// Only draw selection color for selected rows

			if (GetFocusRow()==nRow && GetFocusCell()==nCol)
				break;	// No drawing of selection color for focus cell

			int nImage = GetCellImage(nRow, nCol);
			if (nImage < 0)
				break;

			// Avoid drawing icon with blue background when no focus (gray)
			COLORREF selectColor = ::GetSysColor(COLOR_HIGHLIGHT);
			if (GetFocus()!=this)
			{
				if (GetStyle() & LVS_SHOWSELALWAYS)
					selectColor = ::GetSysColor(COLOR_BTNFACE);
				else
					break;	// no drawing of selection color when not in focus
			}

			CDC* pDC = CDC::FromHandle(pLVCD->nmcd.hdc);

			CRect rcIcon, rcCell;
			VERIFY( GetCellRect(nRow, nCol, LVIR_ICON, rcIcon) );
			VERIFY( GetCellRect(nRow, nCol, LVIR_BOUNDS, rcCell) );

			COLORREF oldBkColor = GetImageList(LVSIL_SMALL)->SetBkColor(selectColor);
			rcCell.right = rcIcon.right + 2;
			pDC->FillSolidRect(&rcCell, selectColor);

			// Draw icon
			GetImageList(LVSIL_SMALL)->Draw (	pDC,  
												nImage,  
												rcIcon.TopLeft(),  
												ILD_BLEND50 );

			GetImageList(LVSIL_SMALL)->SetBkColor(oldBkColor);
		} break;

		case CDDS_ITEMPOSTPAINT:
		{
			// Fix CListCtrl grid drawing bug where vertical grid-border disappears
			//	- To reproduce the bug one needs atleast 2 columns:
			//		1) Resize the second column so a scrollbar appears
			//		2) Scroll to the right so the first column disappear
			//		3) Scroll slowly to the left and the right border of first column is gone
			if ( (GetExtendedStyle() & LVS_EX_GRIDLINES) && (GetExtendedStyle() & LVS_EX_DOUBLEBUFFER))
			{
				CRect rcVisibleRect;
				GetClientRect(rcVisibleRect);

				CDC* pDC = CDC::FromHandle(pLVCD->nmcd.hdc);
				CPen Pen;
				Pen.CreatePen(PS_SOLID, 1, ::GetSysColor(COLOR_BTNFACE));
				CPen* pOldPen = pDC->SelectObject(&Pen);

				int nColCount = GetHeaderCtrl()->GetItemCount();
				for(int nCol = 0; nCol < nColCount; ++nCol)
				{
					CRect rcCell;
					VERIFY( GetCellRect(nRow, nCol, LVIR_BOUNDS, rcCell) );
					if (rcCell.right < 0)
						continue;
					if (rcCell.right > rcVisibleRect.right)
						break;

					pDC->MoveTo(rcCell.right, rcCell.top);
					pDC->LineTo(rcCell.right, rcCell.bottom);
				}

				pDC->SelectObject(pOldPen);
			}
		}
	}

	// Perform the standard drawing as always
	CGridListCtrlGroups::OnCustomDraw(pNMHDR, pResult);

	if (*pResult & CDRF_SKIPDEFAULT)
		return;	// Everything is handled by the column-trait

	*pResult |= CDRF_NOTIFYPOSTPAINT;
}