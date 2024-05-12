#include "pch.h"

#include "CDrawPanel.h"
#include "eck\Env.h"
#include "CDPDefine.h"

HINSTANCE g_hInst;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	g_hInst = hModule;
	return TRUE;
}

BOOL __stdcall CDPInit()
{
	using namespace eck;
	const INITPARAM ip{ EIF_NOINITTHREAD | EIF_NOINITD2D | EIF_NOINITDWRITE | EIF_NODARKMODE };
	return Init(g_hInst, &ip) == InitStatus::Ok;
}

BOOL __stdcall CDPUnInit()
{
	eck::UnInit();
	return TRUE;
}

CDrawPanel* __stdcall CDPCreateDrawPanel(int cx, int cy)
{
	const auto pdp = new CDrawPanel{};
	if (!pdp->Init(cx, cy))
	{
		delete pdp;
		return NULL;
	}
	else
		return pdp;
}

void __stdcall CDPDestroyDrawPanel(CDrawPanel* pdp)
{
	delete pdp;
}

size_t __stdcall CDPGetObjCount(CDrawPanel* pdp, CDPObjType eType)
{
	return pdp->GetObjCount(eType);
}

int __stdcall CDPExecuteCommand(CDrawPanel* pdp, PCWSTR pszCmd, LPARAM lParam)
{
	return pdp->ExecuteCommand(pszCmd, lParam);
}

int __stdcall CDPExecuteCommand2(CDrawPanel* pdp, PCWSTR pszCmd, const CDPIMAGE* pImgIn,
	CDPSTR* pstr, CDPIMAGE* pImgOut, CDPImageMedium eMedium)
{
	return pdp->ExecuteCommand(pszCmd, pImgIn, *pstr, pImgOut, eMedium);
}

void __stdcall CDPGetContent(CDrawPanel* pdp, CDPIMAGE* pImgOut)
{
	switch (pImgOut->eMedium)
	{
	case CDPIM_BIN:
		pImgOut->Bin = pdp->GetImageBin(pImgOut->eType);
		break;
	case CDPIM_HBITMAP:
		pImgOut->hbm = pdp->CloneImageToHBITMAP();
		break;
	case CDPIM_GPIMAGE:
		pImgOut->pBitmap = pdp->CloneImageToGpBitmap();
		break;
	}
}

void __stdcall CDPDestroyStr(const CDPSTR* str)
{
	eck::TRefStrDefAlloc<WCHAR>{}.deallocate(str->psz, str->cchAlloc);
}

void __stdcall CDPDestroyBin(const CDPBIN* bin)
{
	eck::TRefBinDefAllocator{}.deallocate((BYTE*)bin->pData, bin->cbAlloc);
}