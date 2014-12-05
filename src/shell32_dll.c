/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * shell32.dll functions.
  */

#pragma once

#include "win32_utf8.h"
#include "wrappers.h"

const w32u8_pair_t shell32_pairs[] = {
	{"DragQueryFileA", DragQueryFileU},
	{"SHBrowseForFolderA", SHBrowseForFolderU},
	{"SHGetPathFromIDListA", SHGetPathFromIDListU},
	NULL
};

// The HDROP type would be he only reason for #include <shellapi.h> here,
// so let's declare the function ourselves to reduce header bloat.
SHSTDAPI_(UINT) DragQueryFileW(
	__in HANDLE hDrop,
	__in UINT iFile,
	__out_ecount_opt(cch) LPWSTR lpszFile,
	__in UINT cch
);

UINT WINAPI DragQueryFileU(
	__in HANDLE hDrop,
	__in UINT iFile,
	__out_ecount_opt(cch) LPSTR lpszFile,
	__in UINT cch
)
{
	DWORD ret;
	VLA(wchar_t, lpszFile_w, cch);

	if(!lpszFile) {
		VLA_FREE(lpszFile_w);
	}
	ret = DragQueryFileW(hDrop, iFile, lpszFile_w, cch);
	if(ret) {
		if(lpszFile) {
			StringToUTF8(lpszFile, lpszFile_w, cch);
		} else if(iFile != 0xFFFFFFFF) {
			VLA(wchar_t, lpBufferReal_w, ret);
			ret = DragQueryFileW(hDrop, iFile, lpBufferReal_w, cch);
			ret = StringToUTF8(NULL, lpBufferReal_w, 0);
			VLA_FREE(lpBufferReal_w);
		}
	}
	VLA_FREE(lpszFile_w);
	return ret;
}

// CoGetApartmentType() is not available prior to Windows 7, but luckily,
// http://msdn.microsoft.com/en-us/library/windows/desktop/dd542641%28v=vs.85%29.aspx
// describes a way how to get the same functionality on previous systems.
static HRESULT CoGetApartmentTypeCompat(
	_Out_ APTTYPE *apttype
)
{
	int ret = S_FALSE;
	APTTYPEQUALIFIER apttype_qualifier;
	typedef HRESULT WINAPI CoGetApartmentType_t(
		_Out_ APTTYPE *pAptType,
		_Out_ APTTYPEQUALIFIER *pAptQualifier
	);
	// GetVersionEx() is deprecated with Windows 8.1, not everybody will
	// have VersionHelpers.h, and this is a lot more fault-tolerant anyway.
	HMODULE ole32 = LoadLibrary("ole32.dll");
	CoGetApartmentType_t *cgat = NULL;

	*apttype = APTTYPE_MTA;
	if(!ole32) {
		return S_FALSE;
	}
	cgat = (CoGetApartmentType_t*)GetProcAddress(ole32, "CoGetApartmentType");
	if(cgat) {
		ret = cgat(apttype, &apttype_qualifier);
	} else {
		IUnknown *ctx_token = NULL;
		ret = CoGetContextToken((ULONG_PTR*)&ctx_token);
		if(ret == S_OK) {
			IComThreadingInfo *cti = NULL;
			ret = IUnknown_QueryInterface(
				ctx_token, &IID_IComThreadingInfo, (void**)&cti
			);
			if(ret == S_OK) {
				ret = IComThreadingInfo_GetCurrentApartmentType(cti, apttype);
				IUnknown_Release(cti);
			}
		} else if(ret == CO_E_NOTINITIALIZED) {
			*apttype = APTTYPE_CURRENT;
		}
	}
	FreeLibrary(ole32);
	return ret;
}

PIDLIST_ABSOLUTE WINAPI SHBrowseForFolderU(
	__in LPBROWSEINFOA lpbi
)
{
	APTTYPE apttype;
	PIDLIST_ABSOLUTE ret;
	wchar_t pszDisplayName_w[MAX_PATH];
	const char *lpszTitle = lpbi->lpszTitle;
	BROWSEINFOW lpbi_w = *((BROWSEINFOW*)lpbi);
	WCHAR_T_DEC(lpszTitle);
	WCHAR_T_CONV(lpszTitle);

	// Use the new UI if we can
	CoGetApartmentTypeCompat(&apttype);
	if(apttype != APTTYPE_MTA) {
		lpbi_w.ulFlags |= BIF_USENEWUI;
	}
	// Really, folder browse dialogs without edit box should be outlawed.
	lpbi_w.ulFlags |= BIF_EDITBOX;
	lpbi_w.pszDisplayName = pszDisplayName_w;
	lpbi_w.lpszTitle = lpszTitle_w;
	ret = SHBrowseForFolderW(&lpbi_w);
	StringToUTF8(lpbi->pszDisplayName, pszDisplayName_w, MAX_PATH);
	WCHAR_T_FREE(lpszTitle);
	return ret;
}

BOOL WINAPI SHGetPathFromIDListU(
	__in PCIDLIST_ABSOLUTE pidl,
	__out_ecount(MAX_PATH) LPSTR pszPath
)
{
	wchar_t pszPath_w[MAX_PATH];
	BOOL ret = SHGetPathFromIDListW(pidl, pszPath_w);
	if(pszPath) {
		StringToUTF8(pszPath, pszPath_w, MAX_PATH);
		return ret;
	}
	return 0;
}