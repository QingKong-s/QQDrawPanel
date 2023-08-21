#include "Env.h"

#include <Windows.h>
#include <Shlwapi.h>

#include <vector>

#include "GdiplusFlatDef.h"
#include "CRefStr.h"
#include "CAllocator.h"
#include "Utility.h"
#include "DbgHelper.h"

#pragma comment(lib,"msimg32.lib")

HBITMAP m_hBitmap = NULL;
HDC m_hCDC = NULL;
HGDIOBJ m_hOldBmp = NULL;
GpGraphics* m_pGraphics = NULL;

ULONG_PTR m_uGpToken = 0u;
CLSID m_clsidPngEncoder{};

std::vector<GpBrush*> m_Brushes{};
std::vector<GpPen*> m_Pens{};
std::vector<GpFont*> m_Fonts{};

SIZE_T m_idxCurrBrush = -1;
SIZE_T m_idxCurrPen = -1;
SIZE_T m_idxCurrFont = -1;

int m_cx = 0, m_cy = 0;

#define QDPE_OK				0
#define QDPE_PARAM_COUNT	1
#define QDPE_INVALID_PARAM	2
#define QDPE_NO_PEN			3
#define QDPE_NO_BRUSH		4
#define QDPE_OUT_OF_RANGE	5
#define QDPE_NULL_ARRAY		6
#define QDPE_INVALID_CMD	7
#define QDPE_NO_FONT		8
#define QDPE_SO_LARGE_SIZE	9
#define QDPE_GDIP_ERROR		10


struct QDPBIN
{
	void* pData;
	SIZE_T cbSize;
};

BOOL GetEncoderClsid(PCWSTR pszMime, CLSID& clsid)
{
	UINT cEncoder, cbTotal;
	if (GdipGetImageEncodersSize(&cEncoder, &cbTotal) != Ok)
		return FALSE;
	auto pEncoder = (ImageCodecInfo*)eck::CAllocator<BYTE>::Alloc(cbTotal);
	if (GdipGetImageEncoders(cEncoder, cbTotal, pEncoder) != Ok)
	{
		eck::CAllocator<BYTE>::Free((BYTE*)pEncoder);
		return FALSE;
	}

	BOOL b = FALSE;
	EckCounter(cEncoder, i)
	{
		if (wcscmp(pEncoder[i].MimeType, pszMime) == 0)
		{
			clsid = pEncoder[i].Clsid;
			b = TRUE;
			break;
		}
	}
	eck::CAllocator<BYTE>::Free((BYTE*)pEncoder);
	return b;
}

void __stdcall QDPUnInit()
{
	if (m_hCDC)
	{
		GdipDeleteGraphics(m_pGraphics);
		DeleteObject(SelectObject(m_hCDC, m_hOldBmp));
		DeleteDC(m_hCDC);
		m_hCDC = NULL;
		m_hBitmap = NULL;
		m_hOldBmp = NULL;
	}

	for (auto x : m_Pens)
		GdipDeletePen(x);
	for (auto x : m_Brushes)
		GdipDeleteBrush(x);
	for (auto x : m_Fonts)
		GdipDeleteFont(x);
	m_Pens.clear();
	m_Brushes.clear();
	m_Fonts.clear();
	m_cx = m_cy = 0;
}

BOOL __stdcall QDPInit()
{
	GdiplusStartupInput gpsi{};
	gpsi.GdiplusVersion = 1;
	if (GdiplusStartup(&m_uGpToken, &gpsi, NULL) != Ok)
		return FALSE;
	if (!GetEncoderClsid(L"image/png", m_clsidPngEncoder))
		return FALSE;

	GpSolidFill* pBrush;
	GdipCreateSolidFill(0xFFA5CBDB, &pBrush);
	m_Brushes.push_back(pBrush);
	m_idxCurrBrush = 0;

	GpPen* pPen;
	GdipCreatePen1(0xFF000000, 2.f, UnitPixel, &pPen);
	m_Pens.push_back(pPen);
	m_idxCurrPen = 0;

	GpFontFamily* pFontFamily;
	GdipCreateFontFamilyFromName(L"微软雅黑", NULL, &pFontFamily);
	GpFont* pFont;
	GdipCreateFont(pFontFamily, 40.f, FontStyleRegular, UnitPixel, &pFont);
	GdipDeleteFontFamily(pFontFamily);
	m_Fonts.push_back(pFont);
	m_idxCurrFont = 0;
	return TRUE;
}

BOOL __stdcall QDPInitDrawPanel(int cx, int cy)
{
	QDPUnInit();

	HDC hDC = GetDC(NULL);
	m_hCDC = CreateCompatibleDC(hDC);
	m_hBitmap = CreateCompatibleBitmap(hDC, cx, cy);
	m_hOldBmp = SelectObject(m_hCDC, m_hBitmap);
	ReleaseDC(NULL, hDC);

	m_cx = cx;
	m_cy = cy;

	GdipCreateFromHDC(m_hCDC, &m_pGraphics);
	RECT rc{ 0,0,cx,cy };
	FillRect(m_hCDC, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
	return TRUE;
}

int __stdcall QDPAnalyzeColorCommand(PCWSTR pszCmd, ARGB* pargb)
{
	if (wcsncmp(pszCmd, L"ARGB(", 5) == 0)
	{
		eck::CRefStrW rs = pszCmd + 5;
		rs.PopBack(1);
		std::vector<PWSTR> aResult;
		eck::SplitStr(rs, L",", aResult, 0, rs.m_cchText, 1);
		if (aResult.size() != 4)
			return QDPE_PARAM_COUNT;
		auto pClr = (BYTE*)pargb;
		int i;
		StrToIntExW(aResult[3], STIF_SUPPORT_HEX, &i);
		pClr[0] = (BYTE)i;
		StrToIntExW(aResult[2], STIF_SUPPORT_HEX, &i);
		pClr[1] = (BYTE)i;
		StrToIntExW(aResult[1], STIF_SUPPORT_HEX, &i);
		pClr[2] = (BYTE)i;
		StrToIntExW(aResult[0], STIF_SUPPORT_HEX, &i);
		pClr[3] = (BYTE)i;
		return QDPE_OK;
	}
	else
	{
		LONGLONG ll;
		if (!StrToInt64ExW(pszCmd, STIF_SUPPORT_HEX, &ll))
			return QDPE_INVALID_PARAM;
		*pargb = (ARGB)ll;
		return QDPE_OK;
	}
}

SIZE_T __stdcall QDPGetObjCount(int i)
{
	switch (i)
	{
	case 0:
		return  m_Pens.size();
	case 1:
		return m_Brushes.size();
	case 2:
		return m_Fonts.size();
	}
	return 0u;
}

BOOL GetParamList(PCWSTR pszParamListStart, eck::CRefStrW& rs, std::vector<PWSTR>& aResult)
{
	rs = eck::AllTrimStr(pszParamListStart);
	rs.PopBack(1);

	PWSTR pszFind = wcsstr(rs, L",");
	PWSTR pszPrevFirst = rs;
	int pos;
	while (pszFind)
	{
		aResult.push_back(pszPrevFirst);
		if (wcsncmp(pszPrevFirst, L"ARGB(", 5) == 0)
		{
			pos = eck::FindStr(pszPrevFirst, L")");
			if (pos == eck::INVALID_STR_POS)
				return FALSE;
			pszPrevFirst += (pos+1);
			*pszPrevFirst = L'\0';
			++pszPrevFirst;
		}
		else if (wcsncmp(pszPrevFirst, L"{", 1) == 0)
		{
			pos = eck::FindStr(pszPrevFirst, L"}");
			if (pos == eck::INVALID_STR_POS)
				return FALSE;
			pszPrevFirst += (pos+1);
			*pszPrevFirst = L'\0';
			++pszPrevFirst;
		}
		else
		{
			*pszFind = L'\0';
			pszPrevFirst = pszFind + 1;
		}

		if (!*pszPrevFirst)
			return TRUE;
		pszFind = wcsstr(pszPrevFirst, L",");
	}

	if (!*pszPrevFirst)
		return TRUE;
	aResult.push_back(pszPrevFirst);
	return TRUE;
}

void AnalyzeArrayPoint(PCWSTR psz, std::vector<GpPoint>& aResult)
{
	eck::CRefStrW rs = psz + 1;
	rs.PopBack(1);
	std::vector<PWSTR> aMember{};
	eck::SplitStr(rs, L",", aMember, 0, rs.m_cchText, 1);
	for (SIZE_T i = 0; i < aMember.size(); i += 2)
	{
		aResult.push_back({ _wtoi(aMember[i]),_wtoi(aMember[i + 1]) });
	}
}

void AnalyzeArrayFloat(PCWSTR psz, std::vector<REAL>& aResult)
{
	eck::CRefStrW rs = psz + 1;
	rs.PopBack(1);
	if (!rs.m_cchText)
		return;
	std::vector<PWSTR> aMember{};
	eck::SplitStr(rs, L",", aMember, 0, rs.m_cchText, 1);
	EckCounter(aMember.size(), i)
	{
		aResult.push_back((float)_wtof(aMember[i]));
	}
}

#define QDPDECLCMD(CmdName) \
constexpr WCHAR szCmd_##CmdName[] = \
L"." \
ECKTOSTRW___(CmdName) \
L"(" ;\
constexpr int cchCmd_##CmdName = (int)ARRAYSIZE(szCmd_##CmdName) - 1;

struct QDPGETIMAGESIZE
{
	QDPBIN Bin;
	UINT cx;
	UINT cy;
};

struct QDPDRAWIMAGE
{
	QDPBIN Bin;
};

struct QDPLISTOBJ
{
	GpBitmap* pBitmap;
};

struct QDPGETOBJCOUNT
{
	SIZE_T cObj;
};

struct QDPGETSTRINGSIZE
{
	PCWSTR pszText;
	int cx;
	int cy;
	int cchFitted;
	int cLinesFilled;
};

struct QDPREMOVECLR
{
	QDPBIN Bin;
	GpBitmap* pBitmap;
	HBITMAP hBitmap;
};


GpBitmap* CreateBitmapFromQDPBIN(QDPBIN Bin)
{
	IStream* pStream = SHCreateMemStream((const BYTE*)Bin.pData, (UINT)Bin.cbSize);
	GpBitmap* pBitmap;
	GdipCreateBitmapFromStream(pStream, &pBitmap);
	pStream->Release();
	return pBitmap;
}

void ResetBitmapDpi(GpBitmap* pBitmap)
{
	REAL xDpi, yDpi;
	GdipGetDpiX(m_pGraphics, &xDpi);
	GdipGetDpiY(m_pGraphics, &yDpi);
	GdipBitmapSetResolution(pBitmap, xDpi, yDpi);
}

BOOL ToBool(PCWSTR pszText)
{
	return wcsncmp(pszText, L"真", 1) == 0 || wcsnicmp(pszText, L"true", 4) == 0;
}

PWSTR GpMatrixToString(GpMatrix* pMatrix, int& cch)
{
	eck::CRefStrW rs;
	rs.Reserve(30);
	rs.DupString(L"{");
	REAL fElem[6];
	GdipGetMatrixElements(pMatrix, fElem);
	for (auto x : fElem)
	{
		rs.PushBack(eck::ToStr(x));
		rs.PushBack(L",");
	}
	rs[rs.m_cchText - 1] = L'}';
	cch = rs.m_cchText;
	return rs.Detach();
}

int __stdcall QDPExecuteCommand(PCWSTR pszCmd, LPARAM lParam)
{
	QDPDECLCMD(Brush);
	QDPDECLCMD(Pen);
	QDPDECLCMD(ListBrush);
	QDPDECLCMD(ListPen);
	QDPDECLCMD(SelBrush);
	QDPDECLCMD(SelPen);
	QDPDECLCMD(DrawPixel);
	QDPDECLCMD(DrawLine);
	QDPDECLCMD(DrawPolyLine);
	QDPDECLCMD(DrawRect);
	QDPDECLCMD(DrawArc);
	QDPDECLCMD(DrawEllipse);
	QDPDECLCMD(DrawPie);
	QDPDECLCMD(DrawPolygon);
	QDPDECLCMD(FillRect);
	QDPDECLCMD(FillEllipse);
	QDPDECLCMD(FillPie);
	QDPDECLCMD(FillPolygon);
	QDPDECLCMD(GetImageSize);
	QDPDECLCMD(DrawImage);
	QDPDECLCMD(GetObjCount);
	QDPDECLCMD(Clear);
	QDPDECLCMD(SetTransform);
	QDPDECLCMD(MultiplyTransform);
	QDPDECLCMD(FillText);
	QDPDECLCMD(Font);
	QDPDECLCMD(SelFont);
	QDPDECLCMD(ListFont);
	QDPDECLCMD(ReSize); 
	QDPDECLCMD(MatrixTranslate);
	QDPDECLCMD(MatrixShear);
	QDPDECLCMD(MatrixRotate);
	QDPDECLCMD(MatrixScale);
	QDPDECLCMD(MatrixInvert);
	QDPDECLCMD(GetStringSize);
	QDPDECLCMD(DelBrush);
	QDPDECLCMD(DelPen);
	QDPDECLCMD(DelFont);

	int x1, y1, x2, y2;
	int iRet;
	int cxPen;
	REAL f1, f2;
	ARGB argb1, argb2;

	eck::CRefStrW rsTemp{};
	std::vector<PWSTR> aResult{};
	std::vector<GpPoint> aPoints{};
	std::vector<REAL> aFloat; {};
	//////////////////////////////////////////
	if (wcsncmp(pszCmd, szCmd_ReSize, cchCmd_ReSize) == 0)
	{
		GetParamList(pszCmd + cchCmd_ReSize, rsTemp, aResult);
		if (aResult.size() != 2)
			return QDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		if (x1 <= 0 || y1 <= 0)
			return QDPE_INVALID_PARAM;
		constexpr int
			c_cxMax = 1000,
			c_cyMax = 1000;
		if (x1 > c_cxMax || y1 > c_cyMax)
			return QDPE_SO_LARGE_SIZE;

		QDPInitDrawPanel(x1, y1);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_Brush, cchCmd_Brush) == 0)
	{
		GetParamList(pszCmd + cchCmd_Brush, rsTemp, aResult);
		switch (aResult.size())
		{
		case 1u:
		{
			iRet = QDPAnalyzeColorCommand(aResult[0], &argb1);
			if (iRet != QDPE_OK)
				return iRet;
			GpSolidFill* pBrush;
			GdipCreateSolidFill(argb1, &pBrush);
			m_Brushes.push_back(pBrush);
		}
		return QDPE_OK;
		case 6u:
		{
			iRet = QDPAnalyzeColorCommand(aResult[4], &argb1);
			if (iRet != QDPE_OK)
				return iRet;
			iRet = QDPAnalyzeColorCommand(aResult[5], &argb2);
			if (iRet != QDPE_OK)
				return iRet;

			GpPoint
				pt1{ _wtoi(aResult[0]),_wtoi(aResult[1]) },
				pt2{ _wtoi(aResult[2]),_wtoi(aResult[3]) };
			GpLineGradient* pLineGradien;
			GdipCreateLineBrushI(&pt1, &pt2, argb1, argb2, WrapModeTile, &pLineGradien);
			m_Brushes.push_back(pLineGradien);
		}
		return QDPE_OK;
		}
		return QDPE_PARAM_COUNT;
	}
	else if (wcsncmp(pszCmd, szCmd_Pen, cchCmd_Pen) == 0)
	{
		GetParamList(pszCmd + cchCmd_Pen, rsTemp, aResult);
		if (aResult.size() != 2)
			return QDPE_PARAM_COUNT;

		cxPen = _wtoi(aResult[1]);
		if (cxPen < 0.f || cxPen>500.f)
			return QDPE_INVALID_PARAM;
		iRet = QDPAnalyzeColorCommand(aResult[0], &argb1);
		if (iRet != QDPE_OK)
			return iRet;
		GpPen* pPen;
		GdipCreatePen1(argb1, (REAL)cxPen, UnitPixel, &pPen);
		if (!pPen)
			return QDPE_GDIP_ERROR;
		m_Pens.push_back(pPen);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_Font, cchCmd_Font) == 0)
	{
		GetParamList(pszCmd + cchCmd_Font, rsTemp, aResult);
		if (aResult.size() != 3)
			return QDPE_PARAM_COUNT;
		GpFontFamily* pFontFamily;
		GdipCreateFontFamilyFromName(aResult[0], NULL, &pFontFamily);
		f1 = (REAL)_wtof(aResult[1]);
		if (f1 > 600.f || f1 < 0.f)
			return QDPE_INVALID_PARAM;
		GpFont* pFont;
		GdipCreateFont(pFontFamily, f1, (GpFontStyle)_wtoi(aResult[2]), UnitPixel, &pFont);
		GdipDeleteFontFamily(pFontFamily);
		if (!pFont)
			return QDPE_GDIP_ERROR;
		m_Fonts.push_back(pFont);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_DrawPixel, cchCmd_DrawPixel) == 0)
	{
		GetParamList(pszCmd + cchCmd_DrawPixel, rsTemp, aResult);
		if (aResult.size() != 3)
			return QDPE_PARAM_COUNT;

		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		iRet = QDPAnalyzeColorCommand(aResult[2], &argb1);
		if (iRet != QDPE_OK)
			return iRet;
		SetPixel(m_hCDC, x1, y1, argb1 & 0x00FFFFFF);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_GetImageSize, cchCmd_GetImageSize) == 0)
	{
		auto pCtx = (QDPGETIMAGESIZE*)lParam;
		GpBitmap* pBitmap = CreateBitmapFromQDPBIN(pCtx->Bin);
		if (!pBitmap)
			return QDPE_INVALID_PARAM;
		ResetBitmapDpi(pBitmap);
		GdipGetImageWidth(pBitmap, &pCtx->cx);
		GdipGetImageHeight(pBitmap, &pCtx->cy);
		GdipDisposeImage(pBitmap);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_DrawImage, cchCmd_DrawImage) == 0)
	{
		GetParamList(pszCmd + cchCmd_DrawImage, rsTemp, aResult);
		auto pCtx = (QDPDRAWIMAGE*)lParam;
		GpBitmap* pBitmap;
		switch (aResult.size())
		{
		case 2u:
		{
			pBitmap = CreateBitmapFromQDPBIN(pCtx->Bin);
			if (!pBitmap)
				return QDPE_INVALID_PARAM;
			ResetBitmapDpi(pBitmap);
			GdipDrawImageI(m_pGraphics, pBitmap, _wtoi(aResult[0]), _wtoi(aResult[1]));
			GdipDisposeImage(pBitmap);
		}
		return QDPE_OK;
		case 4u:
		{
			pBitmap = CreateBitmapFromQDPBIN(pCtx->Bin);
			if (!pBitmap)
				return QDPE_INVALID_PARAM;
			ResetBitmapDpi(pBitmap);
			GdipDrawImageRectI(m_pGraphics, pBitmap, _wtoi(aResult[0]), _wtoi(aResult[1]), _wtoi(aResult[2]), _wtoi(aResult[3]));
			GdipDisposeImage(pBitmap);
		}
		return QDPE_OK;
		case 8u:
		{
			pBitmap = CreateBitmapFromQDPBIN(pCtx->Bin);
			if (!pBitmap)
				return QDPE_INVALID_PARAM;
			ResetBitmapDpi(pBitmap);
			GdipDrawImageRectRectI(m_pGraphics, pBitmap, _wtoi(aResult[0]), _wtoi(aResult[1]), _wtoi(aResult[2]), _wtoi(aResult[3]),
				_wtoi(aResult[4]), _wtoi(aResult[5]), _wtoi(aResult[6]), _wtoi(aResult[7]), UnitPixel, NULL, NULL, NULL);
			GdipDisposeImage(pBitmap);
		}
		return QDPE_OK;
		default:
			return QDPE_PARAM_COUNT;
		}
	}
	else if (wcsncmp(pszCmd, szCmd_GetObjCount, cchCmd_GetObjCount) == 0)
	{
		GetParamList(pszCmd + cchCmd_SetTransform, rsTemp, aResult);
		if (aResult.size() != 1)
			return QDPE_PARAM_COUNT;
		auto pCtx = (QDPGETOBJCOUNT*)lParam;
		pCtx->cObj = QDPGetObjCount(_wtoi(aResult[0]));
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_Clear, cchCmd_Clear) == 0)
	{
		GetParamList(pszCmd + cchCmd_Clear, rsTemp, aResult);
		switch (aResult.size())
		{
		case 1u:
			iRet = QDPAnalyzeColorCommand(aResult[0], &argb1);
			if (iRet != QDPE_OK)
				return iRet;
			GdipGraphicsClear(m_pGraphics, argb1);
			return QDPE_OK;
		case 0u:
			GdipGraphicsClear(m_pGraphics, 0xFFFFFFFF);
			return QDPE_OK;
		}
		return QDPE_PARAM_COUNT;
	}
	else if (wcsncmp(pszCmd, szCmd_SetTransform, cchCmd_SetTransform) == 0)
	{
		GetParamList(pszCmd + cchCmd_SetTransform, rsTemp, aResult);
		if (aResult.size() != 1)
			return QDPE_PARAM_COUNT;
		AnalyzeArrayFloat(aResult[0], aFloat);
		switch (aFloat.size())
		{
		case 6u:
		{
			GpMatrix* pMatrix;
			GdipCreateMatrix2(aFloat[0], aFloat[1], aFloat[2], aFloat[3], aFloat[4], aFloat[5], &pMatrix);
			GdipSetWorldTransform(m_pGraphics, pMatrix);
			GdipDeleteMatrix(pMatrix);
		}
		return QDPE_OK;
		case 0u:
			GdipResetWorldTransform(m_pGraphics);
			return QDPE_OK;
		default:
			return QDPE_PARAM_COUNT;
		}
	}
	else if (wcsncmp(pszCmd, szCmd_MultiplyTransform, cchCmd_MultiplyTransform) == 0)
	{
		GetParamList(pszCmd + cchCmd_MultiplyTransform, rsTemp, aResult);
		if (aResult.size() != 2)
			return QDPE_PARAM_COUNT;
		AnalyzeArrayFloat(aResult[0], aFloat);
		if (aFloat.size() != 6)
			return QDPE_PARAM_COUNT;
		GpMatrix* pMatrix;
		GdipCreateMatrix2(aFloat[0], aFloat[1], aFloat[2], aFloat[3], aFloat[4], aFloat[5], &pMatrix);
		GdipMultiplyWorldTransform(m_pGraphics, pMatrix, (GpMatrixOrder)_wtoi(aResult[1]));
		GdipDeleteMatrix(pMatrix);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_SelFont, cchCmd_SelFont) == 0)
	{
		GetParamList(pszCmd + cchCmd_SelFont, rsTemp, aResult);
		if (aResult.size() != 1)
			return QDPE_PARAM_COUNT;
		iRet = _wtoi(aResult[0]);
		if (iRet < 0 || iRet >= (int)m_Fonts.size())
			return QDPE_OUT_OF_RANGE;
		m_idxCurrFont = iRet;
		return QDPE_OK;
	}

	//////////////////////////////////////////矩阵
	if (wcsncmp(pszCmd, szCmd_MatrixTranslate, cchCmd_MatrixTranslate) == 0)
	{
		auto pCtx = (QDPBIN*)lParam;
		GetParamList(pszCmd + cchCmd_MatrixTranslate, rsTemp, aResult);
		if (aResult.size() != 2)
			return QDPE_PARAM_COUNT;
		GpMatrix* pMatrix;
		GdipCreateMatrix(&pMatrix);
		GdipTranslateMatrix(pMatrix, (REAL)_wtof(aResult[0]), (REAL)_wtof(aResult[1]), MatrixOrderAppend);
		
		int cch;
		pCtx->pData = GpMatrixToString(pMatrix, cch);
		pCtx->cbSize = (cch + 1) * sizeof(WCHAR);
		GdipDeleteMatrix(pMatrix);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_MatrixShear, cchCmd_MatrixShear) == 0)
	{
		auto pCtx = (QDPBIN*)lParam;
		GetParamList(pszCmd + cchCmd_MatrixShear, rsTemp, aResult);
		GpMatrix* pMatrix;
		switch (aResult.size())
		{
		case 2u:
		{
			GdipCreateMatrix(&pMatrix);
			GdipShearMatrix(pMatrix, (REAL)_wtof(aResult[0]), (REAL)_wtof(aResult[1]), MatrixOrderAppend);
		}
		break;
		case 4u:
		{
			GdipCreateMatrix(&pMatrix);
			f1 = (float)_wtof(aResult[0]);
			f2 = (float)_wtof(aResult[1]);
			GdipTranslateMatrix(pMatrix, -f1, -f2, MatrixOrderAppend);
			GdipShearMatrix(pMatrix, (REAL)_wtof(aResult[2]), (REAL)_wtof(aResult[3]), MatrixOrderAppend);
			GdipTranslateMatrix(pMatrix, f1, f2, MatrixOrderAppend);
		}
		break;
		default:
			return QDPE_PARAM_COUNT;
		}

		int cch;
		pCtx->pData = GpMatrixToString(pMatrix, cch);
		pCtx->cbSize = (cch + 1) * sizeof(WCHAR);
		GdipDeleteMatrix(pMatrix);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_MatrixRotate, cchCmd_MatrixRotate) == 0)
	{
		auto pCtx = (QDPBIN*)lParam;
		GetParamList(pszCmd + cchCmd_MatrixRotate, rsTemp, aResult);
		GpMatrix* pMatrix;
		switch (aResult.size())
		{
		case 1u:
		{
			GdipCreateMatrix(&pMatrix);
			GdipRotateMatrix(pMatrix, (REAL)_wtof(aResult[0]), MatrixOrderAppend);
		}
		break;
		case 3u:
		{
			GdipCreateMatrix(&pMatrix);
			f1 = (float)_wtof(aResult[0]);
			f2 = (float)_wtof(aResult[1]);
			GdipTranslateMatrix(pMatrix, -f1, -f2, MatrixOrderAppend);
			GdipRotateMatrix(pMatrix, (REAL)_wtof(aResult[2]), MatrixOrderAppend);
			GdipTranslateMatrix(pMatrix, f1, f2, MatrixOrderAppend);
		}
		break;
		default:
			return QDPE_PARAM_COUNT;
		}

		int cch;
		pCtx->pData = GpMatrixToString(pMatrix, cch);
		pCtx->cbSize = (cch + 1) * sizeof(WCHAR);
		GdipDeleteMatrix(pMatrix);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_MatrixScale, cchCmd_MatrixScale) == 0)
	{
		auto pCtx = (QDPBIN*)lParam;
		GetParamList(pszCmd + cchCmd_MatrixScale, rsTemp, aResult);
		GpMatrix* pMatrix;
		switch (aResult.size())
		{
		case 2u:
		{
			GdipCreateMatrix(&pMatrix);
			GdipScaleMatrix(pMatrix, (REAL)_wtof(aResult[0]), (REAL)_wtof(aResult[1]), MatrixOrderAppend);
		}
		break;
		case 4u:
		{
			GdipCreateMatrix(&pMatrix);
			f1 = (float)_wtof(aResult[0]);
			f2 = (float)_wtof(aResult[1]);
			GdipTranslateMatrix(pMatrix, -f1, -f2, MatrixOrderAppend);
			GdipScaleMatrix(pMatrix, (REAL)_wtof(aResult[2]), (REAL)_wtof(aResult[3]), MatrixOrderAppend);
			GdipTranslateMatrix(pMatrix, f1, f2, MatrixOrderAppend);
		}
		break;
		default:
			return QDPE_PARAM_COUNT;
		}

		int cch;
		pCtx->pData = GpMatrixToString(pMatrix, cch);
		pCtx->cbSize = (cch + 1) * sizeof(WCHAR);
		GdipDeleteMatrix(pMatrix);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_MatrixInvert, cchCmd_MatrixInvert) == 0)
	{
		auto pCtx = (QDPBIN*)lParam;
		GetParamList(pszCmd + cchCmd_MatrixInvert, rsTemp, aResult);
		if (aResult.size() != 1)
			return QDPE_PARAM_COUNT;
		AnalyzeArrayFloat(aResult[0], aFloat);
		if (aFloat.size() != 6)
			return QDPE_PARAM_COUNT;
		GpMatrix* pMatrix;
		GdipCreateMatrix2(aFloat[0], aFloat[1], aFloat[2], aFloat[3], aFloat[4], aFloat[5], &pMatrix);
		GdipInvertMatrix(pMatrix);
		int cch;
		pCtx->pData = GpMatrixToString(pMatrix, cch);
		pCtx->cbSize = (cch + 1) * sizeof(WCHAR);
		GdipDeleteMatrix(pMatrix);
		return QDPE_OK;
	}

	//////////////////////////////////////////下面的指令需要画笔
	if (wcsncmp(pszCmd, szCmd_ListPen, cchCmd_ListPen) == 0)
		{
			auto pCtx = (QDPLISTOBJ*)lParam;
			GetParamList(pszCmd + cchCmd_ListPen, rsTemp, aResult);
			argb1 = 0xFFFFFFFF;
			if (aResult.size() == 1)
			{
				iRet = QDPAnalyzeColorCommand(aResult[0], &argb1);
				if (iRet != QDPE_OK)
					return iRet;
			}
			else if (aResult.size() != 0)
				return QDPE_INVALID_PARAM;
			constexpr int
				c_cyPadding = 12,
				c_cxPadding = 10,
				c_cxBitmap = 100;
			int cy = (((int)m_Pens.size()) + 1) * c_cyPadding;
			REAL cxPen;
			for (auto x : m_Pens)
			{
				GdipGetPenWidth(x, &cxPen);
				cy += (int)cxPen;
			}

			GpBitmap* pBitmap;
			GpGraphics* pGraphics;
			GdipCreateBitmapFromScan0(c_cxBitmap, cy, 0, PixelFormat32bppPARGB, NULL, &pBitmap);
			GdipGetImageGraphicsContext(pBitmap, &pGraphics);
			GdipGraphicsClear(pGraphics, argb1);
			int y = c_cyPadding;
			for (auto p : m_Pens)
			{
				GdipDrawLineI(pGraphics, p, c_cxPadding, y, c_cxBitmap - c_cxPadding, y);
				GdipGetPenWidth(p, &cxPen);
				y += ((int)cxPen + c_cyPadding);
			}
			GdipDeleteGraphics(pGraphics);
			pCtx->pBitmap = pBitmap;
			return QDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_DelPen, cchCmd_DelPen) == 0)
	{
		GetParamList(pszCmd + cchCmd_DelPen, rsTemp, aResult);
		switch (aResult.size())
		{
		case 1u:
			iRet = _wtoi(aResult[0]);
			if (iRet < 0 || iRet >= (int)m_Pens.size())
				return QDPE_OUT_OF_RANGE;
			GdipDeletePen(m_Pens[iRet]);
			m_Pens.erase(m_Pens.begin() + iRet);
			return QDPE_OK;
		case 0u:
			for (auto x : m_Pens)
				GdipDeletePen(x);
			m_Pens.clear();
			m_idxCurrPen = -1;
			return QDPE_OK;
		}
		return QDPE_PARAM_COUNT;
	}
	else if (wcsncmp(pszCmd, szCmd_SelPen, cchCmd_SelPen) == 0)
	{
		GetParamList(pszCmd + cchCmd_SelPen, rsTemp, aResult);
		if (aResult.size() != 1)
			return QDPE_PARAM_COUNT;
		iRet = _wtoi(aResult[0]);
		if (iRet < 0 || iRet >= (int)m_Pens.size())
			return QDPE_OUT_OF_RANGE;
		m_idxCurrPen = iRet;
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_DrawLine, cchCmd_DrawLine) == 0)// .DrawLine(x1,y1,x2,y2,clr,width)
	{
		if (m_idxCurrPen >= m_Pens.size())
			return QDPE_NO_PEN;
		GetParamList(pszCmd + cchCmd_DrawLine, rsTemp, aResult);
		if (aResult.size() != 4)
			return QDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);

		GdipDrawLineI(m_pGraphics, m_Pens[m_idxCurrPen], x1, y1, x2, y2);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_DrawPolyLine, cchCmd_DrawPolyLine) == 0)
	{
		if (m_idxCurrPen >= m_Pens.size())
			return QDPE_NO_PEN;
		GetParamList(pszCmd + cchCmd_DrawPolyLine, rsTemp, aResult);
		if (aResult.size() != 1)
			return QDPE_PARAM_COUNT;
		AnalyzeArrayPoint(aResult[0], aPoints);
		if (!aPoints.size())
			return QDPE_NULL_ARRAY;
		auto i =GdipDrawLinesI(m_pGraphics, m_Pens[m_idxCurrPen], aPoints.data(), (int)aPoints.size());
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_DrawRect, cchCmd_DrawRect) == 0)
	{
		if (m_idxCurrPen >= m_Pens.size())
			return QDPE_NO_PEN;
		GetParamList(pszCmd + cchCmd_DrawRect, rsTemp, aResult);
		if (aResult.size() != 4)
			return QDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);

		GdipDrawRectangleI(m_pGraphics, m_Pens[m_idxCurrPen], x1, y1, x2, y2);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_DrawArc, cchCmd_DrawArc) == 0)
	{
		if (m_idxCurrPen >= m_Pens.size())
			return QDPE_NO_PEN;
		GetParamList(pszCmd + cchCmd_DrawArc, rsTemp, aResult);
		if (aResult.size() != 6)
			return QDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);
		
		f1 = (float)_wtof(aResult[4]);
		f2 = (float)_wtof(aResult[5]);

		GdipDrawArcI(m_pGraphics, m_Pens[m_idxCurrPen], x1, y1, x2, y2, f1, f2);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_DrawEllipse, cchCmd_DrawEllipse) == 0)
	{
		if (m_idxCurrPen >= m_Pens.size())
			return QDPE_NO_PEN;
		GetParamList(pszCmd + cchCmd_DrawEllipse, rsTemp, aResult);
		if (aResult.size() != 4)
			return QDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);

		GdipDrawEllipseI(m_pGraphics, m_Pens[m_idxCurrPen], x1, y1, x2, y2);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_DrawPie, cchCmd_DrawPie) == 0)
	{
		if (m_idxCurrPen >= m_Pens.size())
			return QDPE_NO_PEN;
		GetParamList(pszCmd + cchCmd_DrawPie, rsTemp, aResult);
		if (aResult.size() != 6)
			return QDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);

		f1 = (float)_wtof(aResult[4]);
		f2 = (float)_wtof(aResult[5]);

		GdipDrawPieI(m_pGraphics, m_Pens[m_idxCurrPen], x1, y1, x2, y2, f1, f2);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_DrawPolygon, cchCmd_DrawPolygon) == 0)
	{
		if (m_idxCurrPen >= m_Pens.size())
			return QDPE_NO_PEN;
		GetParamList(pszCmd + cchCmd_DrawPolygon, rsTemp, aResult);
		if (aResult.size() != 1)
			return QDPE_PARAM_COUNT;
		AnalyzeArrayPoint(aResult[0], aPoints);
		if (!aPoints.size())
			return QDPE_NULL_ARRAY;
		GdipDrawPolygonI(m_pGraphics, m_Pens[m_idxCurrPen], aPoints.data(), (int)aPoints.size());
		return QDPE_OK;
	}

	//////////////////////////////////////////下面的指令需要画刷
	if (wcsncmp(pszCmd, szCmd_ListBrush, cchCmd_ListBrush) == 0)
	{
		auto pCtx = (QDPLISTOBJ*)lParam;
		GetParamList(pszCmd + cchCmd_ListBrush, rsTemp, aResult);
		argb1 = 0xFFFFFFFF;
		if (aResult.size() == 1)
		{
			iRet = QDPAnalyzeColorCommand(aResult[0], &argb1);
			if (iRet != QDPE_OK)
				return iRet;
		}
		else if (aResult.size() != 0)
			return QDPE_INVALID_PARAM;
		constexpr int
			c_cyPadding = 12,
			c_cxPadding = 10,
			c_cxBitmap = 100,
			c_cyBlock = 20;
		int cy = ((int)m_Brushes.size() * (c_cyBlock + c_cyPadding)) + c_cyPadding;

		GpBitmap* pBitmap;
		GpGraphics* pGraphics;
		GdipCreateBitmapFromScan0(c_cxBitmap, cy, 0, PixelFormat32bppPARGB, NULL, &pBitmap);
		GdipGetImageGraphicsContext(pBitmap, &pGraphics);
		GdipGraphicsClear(pGraphics, argb1);
		int y = c_cyPadding;
		for (auto p : m_Brushes)
		{
			GdipFillRectangleI(pGraphics, p, c_cxPadding, y, c_cxBitmap - c_cxPadding * 2, c_cyBlock);
			y += (c_cyPadding + c_cyBlock);
		}
		GdipDeleteGraphics(pGraphics);
		pCtx->pBitmap = pBitmap;
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_DelBrush, cchCmd_DelBrush) == 0)
	{
		GetParamList(pszCmd + cchCmd_DelBrush, rsTemp, aResult);
		switch (aResult.size())
		{
		case 1u:
			iRet = _wtoi(aResult[0]);
			if (iRet < 0 || iRet >= (int)m_Brushes.size())
				return QDPE_OUT_OF_RANGE;
			GdipDeleteBrush(m_Brushes[iRet]);
			m_Brushes.erase(m_Brushes.begin() + iRet);
			return QDPE_OK;
		case 0u:
			for (auto x : m_Brushes)
				GdipDeleteBrush(x);
			m_Brushes.clear();
			m_idxCurrBrush = -1;
			return QDPE_OK;
		}
		return QDPE_PARAM_COUNT;
	}
	else if (wcsncmp(pszCmd, szCmd_SelBrush, cchCmd_SelBrush) == 0)
	{
		GetParamList(pszCmd + cchCmd_SelBrush, rsTemp, aResult);
		if (aResult.size() != 1)
			return QDPE_PARAM_COUNT;
		iRet = _wtoi(aResult[0]);
		if (iRet < 0 || iRet >= (int)m_Brushes.size())
			return QDPE_OUT_OF_RANGE;
		m_idxCurrBrush = iRet;
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_FillRect, cchCmd_FillRect) == 0)
	{
		if (m_idxCurrBrush >= m_Brushes.size())
			return QDPE_NO_BRUSH;
		GetParamList(pszCmd + cchCmd_FillRect, rsTemp, aResult);
		if (aResult.size() != 4)
			return QDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);

		GdipFillRectangleI(m_pGraphics, m_Brushes[m_idxCurrBrush], x1, y1, x2, y2);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_FillEllipse, cchCmd_FillEllipse) == 0)
	{
		if (m_idxCurrBrush >= m_Brushes.size())
			return QDPE_NO_BRUSH;
		GetParamList(pszCmd + cchCmd_FillEllipse, rsTemp, aResult);
		if (aResult.size() != 4)
			return QDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);

		GdipFillEllipseI(m_pGraphics, m_Brushes[m_idxCurrBrush], x1, y1, x2, y2);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_FillPie, cchCmd_FillPie) == 0)
	{
		if (m_idxCurrBrush >= m_Brushes.size())
			return QDPE_NO_BRUSH;
		GetParamList(pszCmd + cchCmd_FillPie, rsTemp, aResult);
		if (aResult.size() != 6)
			return QDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);

		f1 = (float)_wtof(aResult[4]);
		f2 = (float)_wtof(aResult[5]);

		GdipFillPieI(m_pGraphics, m_Brushes[m_idxCurrBrush], x1, y1, x2, y2, f1, f2);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_FillPolygon, cchCmd_FillPolygon) == 0)
	{
		if (m_idxCurrBrush >= m_Brushes.size())
			return QDPE_NO_BRUSH;
		GetParamList(pszCmd + cchCmd_FillPolygon, rsTemp, aResult);
		if (aResult.size() != 1)
			return QDPE_PARAM_COUNT;
		AnalyzeArrayPoint(aResult[0], aPoints);
		if (!aPoints.size())
			return QDPE_NULL_ARRAY;

		std::vector<BYTE> aTypes(aPoints.size(), PathPointTypeLine);
		GpPath* pPath;
		GdipCreatePath2I(aPoints.data(), aTypes.data(), (int)aPoints.size(), FillModeWinding, &pPath);
		GdipFillPath(m_pGraphics, m_Brushes[m_idxCurrBrush], pPath);
		GdipDeletePath(pPath);
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_FillText, cchCmd_FillText) == 0)
	{
		if (m_idxCurrBrush >= m_Brushes.size())
			return QDPE_NO_BRUSH;
		if (m_idxCurrFont >= m_Fonts.size())
			return QDPE_NO_FONT;
		GetParamList(pszCmd + cchCmd_FillText, rsTemp, aResult);
		if (aResult.size() != 7)
			return QDPE_PARAM_COUNT;
		PCWSTR pszText = (PCWSTR)lParam;
		GpRectF rcF{ (REAL)_wtof(aResult[0]),(REAL)_wtof(aResult[1]),(REAL)_wtof(aResult[2]),(REAL)_wtof(aResult[3]) };
		GpStringFormat* pStringFormat;
		GdipCreateStringFormat(ToBool(aResult[4]) ? (GpStringFormatFlags)0 : StringFormatFlagsNoWrap, 
			LANG_NEUTRAL, &pStringFormat);
		GdipSetStringFormatAlign(pStringFormat, (StringAlignment)_wtoi(aResult[5]));
		GdipSetStringFormatLineAlign(pStringFormat, (StringAlignment)_wtoi(aResult[6]));
		
		GdipDrawString(m_pGraphics, pszText, -1, m_Fonts[m_idxCurrFont], 
			&rcF, pStringFormat, m_Brushes[m_idxCurrBrush]);
		GdipDeleteStringFormat(pStringFormat);
		return QDPE_OK;
	}

	//////////////////////////////////////////下面的指令需要字体
	if (wcsncmp(pszCmd, szCmd_ListFont, cchCmd_ListFont) == 0)
	{
		auto pCtx = (QDPLISTOBJ*)lParam;

		constexpr int
			c_cyPadding = 12,
			c_cxPadding = 10;
		constexpr PCWSTR c_pszTest = L"字体 Font 123 ";
		int cy = (((int)m_Brushes.size()) + 1) * c_cyPadding;

		GpRectF rcF{ 0,0,1000.f,500.f }, rcF2;

		GpStringFormat* pStringFormat;
		GdipCreateStringFormat(StringFormatFlagsNoClip | StringFormatFlagsMeasureTrailingSpaces, LANG_NEUTRAL, &pStringFormat);
		GdipSetStringFormatLineAlign(pStringFormat, StringAlignmentCenter);

		std::vector<REAL> aHeight(m_Fonts.size());

		int cx = INT_MIN;
		EckCounter(m_Fonts.size(), i)
		{
			GdipMeasureString(m_pGraphics, c_pszTest, -1, m_Fonts[i], &rcF, pStringFormat, &rcF2, NULL, NULL);
			aHeight[i] = rcF2.Height;
			cy += (int)rcF2.Height;
			if (cx < (int)rcF2.Width)
				cx = (int)rcF2.Width;
		}

		GpBitmap* pBitmap;
		GpGraphics* pGraphics;
		GdipCreateBitmapFromScan0(cx, cy, 0, PixelFormat32bppPARGB, NULL, &pBitmap);
		GdipGetImageGraphicsContext(pBitmap, &pGraphics);
		GdipGraphicsClear(pGraphics, 0xFFFFFFFF);
		int y = c_cyPadding;
		rcF = { 0,0,(REAL)cx,0 };

		GpSolidFill* pBrush;
		GdipCreateSolidFill(0xFF000000, &pBrush);

		EckCounter(m_Fonts.size(), i)
		{
			rcF.Top = (REAL)y;
			rcF.Height = aHeight[i];
			GdipDrawString(pGraphics, c_pszTest, -1, m_Fonts[i], &rcF, pStringFormat, pBrush);

			y += ((int)rcF.Height + c_cyPadding);
		}
		GdipDeleteStringFormat(pStringFormat);
		GdipDeleteBrush(pBrush);
		GdipDeleteGraphics(pGraphics);
		pCtx->pBitmap = pBitmap;
		return QDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_DelFont, cchCmd_DelFont) == 0)
	{
		GetParamList(pszCmd + cchCmd_DelFont, rsTemp, aResult);
		switch (aResult.size())
		{
		case 1u:
			iRet = _wtoi(aResult[0]);
			if (iRet < 0 || iRet >= (int)m_Fonts.size())
				return QDPE_OUT_OF_RANGE;
			GdipDeleteFont(m_Fonts[iRet]);
			m_Fonts.erase(m_Fonts.begin() + iRet);
			return QDPE_OK;
		case 0u:
			for (auto x : m_Fonts)
				GdipDeleteFont(x);
			m_Fonts.clear();
			m_idxCurrFont = -1;
			return QDPE_OK;
		}
		return QDPE_PARAM_COUNT;
	}
	if (wcsncmp(pszCmd, szCmd_GetStringSize, cchCmd_GetStringSize) == 0)
	{
		auto pCtx = (QDPGETSTRINGSIZE*)lParam;
		if (m_idxCurrFont >= m_Fonts.size())
			return QDPE_NO_FONT;
		GetParamList(pszCmd + cchCmd_GetStringSize, rsTemp, aResult);
		if (aResult.size() != 7)
			return QDPE_PARAM_COUNT;

		GpRectF rcF{ (REAL)_wtof(aResult[0]),(REAL)_wtof(aResult[1]),(REAL)_wtof(aResult[2]),(REAL)_wtof(aResult[3]) }, rcF2;
		GpStringFormat* pStringFormat;
		GdipCreateStringFormat(ToBool(aResult[4]) ? (GpStringFormatFlags)0 : StringFormatFlagsNoWrap,
			LANG_NEUTRAL, & pStringFormat);
		GdipSetStringFormatAlign(pStringFormat, (StringAlignment)_wtoi(aResult[5]));
		GdipSetStringFormatLineAlign(pStringFormat, (StringAlignment)_wtoi(aResult[6]));

		GdipMeasureString(m_pGraphics, pCtx->pszText, -1, m_Fonts[m_idxCurrFont],
			&rcF, pStringFormat, &rcF2, &pCtx->cchFitted, &pCtx->cLinesFilled);
		pCtx->cx = (int)rcF2.Width;
		pCtx->cy = (int)rcF2.Height;
		GdipDeleteStringFormat(pStringFormat);
	}
	return QDPE_INVALID_CMD;
}

IStream* __stdcall QDPGetImageData(GpBitmap* pBitmap)
{
	BOOL bNeedFree = FALSE;
	if (!pBitmap)
	{
		GdipCreateBitmapFromHBITMAP(m_hBitmap, NULL, &pBitmap);
		bNeedFree = TRUE;
	}
	IStream* pStream = SHCreateMemStream(NULL, 0);
	if (GdipSaveImageToStream(pBitmap, pStream, &m_clsidPngEncoder, NULL) != Ok)
	{
		if (bNeedFree)
			GdipDisposeImage(pBitmap);
		pStream->Release();
		return NULL;
	}

	if (bNeedFree)
		GdipDisposeImage(pBitmap);
	return pStream;
}

DWORD __stdcall QDPGetStreamSize(IStream* pStream)
{
	STATSTG stg;
	pStream->Stat(&stg, STATFLAG_NONAME);
	return stg.cbSize.LowPart;
}

void __stdcall QDPGetStreamData(IStream* pStream, void* pBuf, ULONG cb)
{
	pStream->Seek({ 0 }, STREAM_SEEK_SET, NULL);
	ULONG cbRead;
	pStream->Read(pBuf, cb, &cbRead);
}

void __stdcall QDPReleaseIUnknown(IUnknown* pUnknown)
{
	pUnknown->Release();
}

void __stdcall QDPDestroyImage(GpImage* pImage)
{
	GdipDisposeImage(pImage);
}

void __stdcall QDPFreeRefStr(PWSTR psz)
{
	eck::CRefStrW::TAlloc::Free(psz);
}