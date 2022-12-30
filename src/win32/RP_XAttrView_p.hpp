/***************************************************************************
 * ROM Properties Page shell extension. (Win32)                            *
 * RP_XAttrView_p.hpp: Extended attribute viewer property page.            *
 * (Private class)                                                         *
 *                                                                         *
 * Copyright (c) 2016-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __ROMPROPERTIES_WIN32_RP_XATTRVIEW_P_HPP__
#define __ROMPROPERTIES_WIN32_RP_XATTRVIEW_P_HPP__

// TCHAR
#include "tcharx.h"

/** RP_XAttrView_Private **/
// Workaround for RP_D() expecting the no-underscore naming convention.
#define RP_XAttrViewPrivate RP_XAttrView_Private

class RP_XAttrView_Private
{
	public:
		/**
		 * RP_XAttrView_Private constructor
		 * @param q
		 * @param filename Filename (RP_XAttrView_Private takes ownership)
		 */
		explicit RP_XAttrView_Private(RP_XAttrView *q, LPTSTR filename);

		~RP_XAttrView_Private();

	private:
		RP_DISABLE_COPY(RP_XAttrView_Private)
	private:
		RP_XAttrView *const q_ptr;

	public:
		// Property for "tab pointer".
		// This points to the RP_XAttrView_Private::tab object.
		static const TCHAR TAB_PTR_PROP[];

	public:
		HWND hDlgSheet;		// Property sheet
		LPTSTR filename;	// Opened file

		// wtsapi32.dll for Remote Desktop status. (WinXP and later)
		LibWin32UI::WTSSessionNotification wts;

		/**
		 * ListView CustomDraw function.
		 * @param plvcd	[in/out] NMLVCUSTOMDRAW
		 * @return Return value.
		 */
		inline int ListView_CustomDraw(NMLVCUSTOMDRAW *plvcd) const;

	public:
		// Is the UI locale right-to-left?
		// If so, this will be set to WS_EX_LAYOUTRTL.
		DWORD dwExStyleRTL;

		// Alternate row color.
		COLORREF colorAltRow;
		bool isFullyInit;		// True if the window is fully initialized.

	public:
		/**
		 * Initialize the dialog. (hDlgSheet)
		 * Called by WM_INITDIALOG.
		 */
		void initDialog(void);

	private:
		// Internal functions used by the callback functions.
		INT_PTR DlgProc_WM_NOTIFY(HWND hDlg, NMHDR *pHdr);

	public:
		// Property sheet callback functions.
		static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		static UINT CALLBACK CallbackProc(HWND hWnd, UINT uMsg, LPPROPSHEETPAGE ppsp);

		/**
		 * Dialog procedure for subtabs.
		 * @param hDlg
		 * @param uMsg
		 * @param wParam
		 * @param lParam
		 */
		static INT_PTR CALLBACK SubtabDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif /* __ROMPROPERTIES_WIN32_RP_XATTRVIEW_P_HPP__ */
