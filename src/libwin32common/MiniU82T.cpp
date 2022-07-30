/***************************************************************************
 * ROM Properties Page shell extension. (libwin32common)                   *
 * MiniU82T.cpp: Minimal U82T()/T2U8() functions.                          *
 *                                                                         *
 * Copyright (c) 2016-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#include "MiniU82T.hpp"

// C includes. (C++ namespace)
#include <cassert>

// C++ includes.
#include <string>
using std::string;
using std::wstring;

namespace LibWin32Common {

/**
 * Mini W2U8() function.
 * @param wcs WCHAR string
 * @return UTF-8 C++ string
 */
string W2U8(const wchar_t *wcs)
{
	string s_ret;

	// NOTE: cbMbs includes the NULL terminator.
	int cbMbs = WideCharToMultiByte(CP_UTF8, 0, wcs, -1, nullptr, 0, nullptr, nullptr);
	if (cbMbs <= 1) {
		return s_ret;
	}
	cbMbs--;
 
	char *const mbs = new char[cbMbs];
	WideCharToMultiByte(CP_UTF8, 0, wcs, -1, mbs, cbMbs, nullptr, nullptr);
	s_ret.assign(mbs, cbMbs);
	delete[] mbs;
	return s_ret;
}

/**
 * Mini U82W() function.
 * @param mbs UTF-8 string.
 * @return UTF-16 C++ wstring.
 */
wstring U82W(const char *mbs)
{
	wstring ws_ret;

	// NOTE: cchWcs includes the NULL terminator.
	int cchWcs = MultiByteToWideChar(CP_UTF8, 0, mbs, -1, nullptr, 0);
	if (cchWcs <= 1) {
		return ws_ret;
	}
	cchWcs--;
 
	wchar_t *const wcs = new wchar_t[cchWcs];
	MultiByteToWideChar(CP_UTF8, 0, mbs, -1, wcs, cchWcs);
	ws_ret.assign(wcs, cchWcs);
	delete[] wcs;
	return ws_ret;
}

}
