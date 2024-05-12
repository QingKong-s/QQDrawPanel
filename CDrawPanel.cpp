#include "pch.h"
#include "CDrawPanel.h"

#pragma region 命令定义
CDPDECLCMD(Brush);
CDPDECLCMD(Pen);
CDPDECLCMD(ListBrush);
CDPDECLCMD(ListPen);
CDPDECLCMD(SelBrush);
CDPDECLCMD(SelPen);
CDPDECLCMD(DrawPixel);
CDPDECLCMD(DrawLine);
CDPDECLCMD(DrawPolyLine);
CDPDECLCMD(DrawRect);
CDPDECLCMD(DrawArc);
CDPDECLCMD(DrawEllipse);
CDPDECLCMD(DrawPie);
CDPDECLCMD(DrawPolygon);
CDPDECLCMD(FillRect);
CDPDECLCMD(FillEllipse);
CDPDECLCMD(FillPie);
CDPDECLCMD(FillPolygon);
CDPDECLCMD(GetImageSize);
CDPDECLCMD(DrawImage);
CDPDECLCMD(GetObjCount);
CDPDECLCMD(Clear);
CDPDECLCMD(SetTransform);
CDPDECLCMD(MultiplyTransform);
CDPDECLCMD(FillText);
CDPDECLCMD(Font);
CDPDECLCMD(SelFont);
CDPDECLCMD(ListFont);
CDPDECLCMD(ReSize);
CDPDECLCMD(MatrixTranslate);
CDPDECLCMD(MatrixShear);
CDPDECLCMD(MatrixRotate);
CDPDECLCMD(MatrixScale);
CDPDECLCMD(MatrixInvert);
CDPDECLCMD(GetStringSize);
CDPDECLCMD(DelBrush);
CDPDECLCMD(DelPen);
CDPDECLCMD(DelFont);
#pragma endregion 命令定义

EckInline BOOL ToBool(PCWSTR pszText)
{
	return wcsncmp(pszText, L"真", 1) == 0 ||
		_wcsnicmp(pszText, L"true", 4) == 0 ||
		_wtoi(pszText) != 0;
}

EckInline CDPSTR RefStrToCdpStr(eck::CRefStrW& rs)
{
	CDPSTR str;
	str.psz = rs.Detach(str.cchAlloc, str.cch);
	return str;
}

static eck::CRefStrW GpMatrixToString(GpMatrix* pMatrix)
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
	rs.Back() = L'}';
	return rs;
}

static int AnalyzeColorCommand(PCWSTR pszCmd, ARGB* pargb)
{
	if (wcsncmp(pszCmd, L"ARGB(", 5) == 0)
	{
		eck::CRefStrW rs = pszCmd + 5;
		rs.PopBack(1);
		std::vector<PWSTR> aResult;
		eck::SplitStr(rs.Data(), L",", aResult, 0, rs.Size(), 1);
		if (aResult.size() != 4)
			return CDPE_PARAM_COUNT;
		const auto pClr = (BYTE*)pargb;
		int i;
		StrToIntExW(aResult[3], STIF_SUPPORT_HEX, &i);
		pClr[0] = (BYTE)i;
		StrToIntExW(aResult[2], STIF_SUPPORT_HEX, &i);
		pClr[1] = (BYTE)i;
		StrToIntExW(aResult[1], STIF_SUPPORT_HEX, &i);
		pClr[2] = (BYTE)i;
		StrToIntExW(aResult[0], STIF_SUPPORT_HEX, &i);
		pClr[3] = (BYTE)i;
		return CDPE_OK;
	}
	else
	{
		LONGLONG ll;
		if (!StrToInt64ExW(pszCmd, STIF_SUPPORT_HEX, &ll))
			return CDPE_INVALID_PARAM;
		*pargb = (ARGB)ll;
		return CDPE_OK;
	}
}

static BOOL GetParamList(PCWSTR pszParamListStart, eck::CRefStrW& rs, std::vector<PWSTR>& aResult)
{
	rs = eck::AllTrimStr(pszParamListStart);
	rs.PopBack(1);

	PWSTR pszFind = wcsstr(rs.Data(), L",");
	PWSTR pszPrevFirst = rs.Data();
	int pos;
	while (pszFind)
	{
		aResult.push_back(pszPrevFirst);
		if (wcsncmp(pszPrevFirst, L"ARGB(", 5) == 0)
		{
			pos = eck::FindStr(pszPrevFirst, L")");
			if (pos == eck::INVALID_STR_POS)
				return FALSE;
			pszPrevFirst += (pos + 1);
			*pszPrevFirst = L'\0';
			++pszPrevFirst;
		}
		else if (wcsncmp(pszPrevFirst, L"{", 1) == 0)
		{
			pos = eck::FindStr(pszPrevFirst, L"}");
			if (pos == eck::INVALID_STR_POS)
				return FALSE;
			pszPrevFirst += (pos + 1);
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

static void AnalyzeArrayPoint(PCWSTR psz, std::vector<GpPoint>& aResult)
{
	eck::CRefStrW rs = psz + 1;
	rs.PopBack(1);
	std::vector<PWSTR> aMember{};
	eck::SplitStr(rs.Data(), L",", aMember, 0, rs.Size(), 1);
	for (SIZE_T i = 0; i < aMember.size(); i += 2)
	{
		aResult.push_back({ _wtoi(aMember[i]),_wtoi(aMember[i + 1]) });
	}
}

static void AnalyzeArrayFloat(PCWSTR psz, std::vector<REAL>& aResult)
{
	eck::CRefStrW rs = psz + 1;
	rs.PopBack(1);
	if (!rs.Size())
		return;
	std::vector<PWSTR> aMember{};
	eck::SplitStr(rs.Data(), L",", aMember, 0, rs.Size(), 1);
	EckCounter(aMember.size(), i)
	{
		aResult.push_back((float)_wtof(aMember[i]));
	}
}

static GpBitmap* CreateBitmapFromCDPBIN(const CDPBIN& Bin)
{
	IStream* pStream = new eck::CStreamView(Bin.pData, Bin.cb);
	GpBitmap* pBitmap;
	GdipCreateBitmapFromStream(pStream, &pBitmap);
	pStream->Release();
	return pBitmap;
}


BOOL CDrawPanel::Init(int cx, int cy)
{
	if (m_pGraphics)
		return FALSE;

	m_DC.Create32(NULL, cx, cy);
	GetObjectW(m_DC.GetBitmap(), sizeof(BITMAP), &m_Bmp);

	m_cx = cx;
	m_cy = cy;

	if (GdipCreateFromHDC(m_DC.GetDC(), &m_pGraphics) != Gdiplus::Ok)
		return FALSE;
	GdipSetSmoothingMode(m_pGraphics, Gdiplus::SmoothingModeHighQuality);
	GdipSetTextRenderingHint(m_pGraphics, Gdiplus::TextRenderingHintAntiAlias);
	GdipGraphicsClear(m_pGraphics, 0xFFFFFFFF);

	GpSolidFill* pBrush;
	GdipCreateSolidFill(0xFFA5CBDB, &pBrush);
	m_Brushes.push_back(pBrush);
	m_idxCurrBrush = 0;

	GpPen* pPen;
	GdipCreatePen1(0xFF000000, 2.f, Gdiplus::UnitPixel, &pPen);
	m_Pens.push_back(pPen);
	m_idxCurrPen = 0;

	GpFontFamily* pFontFamily;
	GdipCreateFontFamilyFromName(L"微软雅黑", NULL, &pFontFamily);
	GpFont* pFont;
	GdipCreateFont(pFontFamily, 40.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel, &pFont);
	GdipDeleteFontFamily(pFontFamily);
	m_Fonts.push_back(pFont);
	m_idxCurrFont = 0;
	return TRUE;
}

BOOL CDrawPanel::UnInit()
{
	if (!m_pGraphics)
		return FALSE;

	GdipDeleteGraphics(m_pGraphics);
	m_pGraphics = NULL;
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
	m_DC.Destroy();
	return TRUE;
}

BOOL CDrawPanel::ReSize(int cx, int cy)
{
	eck::CEzCDC DC{};
	DC.Create32(NULL, cx, cy);
	GetObjectW(DC.GetBitmap(), sizeof(BITMAP), &m_Bmp);

	GdipDeleteGraphics(m_pGraphics);
	m_pGraphics = NULL;
	if (GdipCreateFromHDC(DC.GetDC(), &m_pGraphics) != Gdiplus::Ok)
		return FALSE;
	GdipSetSmoothingMode(m_pGraphics, Gdiplus::SmoothingModeHighQuality);
	GdipSetTextRenderingHint(m_pGraphics, Gdiplus::TextRenderingHintAntiAlias);
	GdipGraphicsClear(m_pGraphics, 0xFFFFFFFF);

	const int cxMin = std::min(cx, m_cx),
		cyMin = std::min(cy, m_cy);
	AlphaBlend(DC.GetDC(), 0, 0, cxMin, cyMin, m_DC.GetDC(), 0, 0, cxMin, cyMin,
		{ AC_SRC_OVER,0,255,AC_SRC_ALPHA });

	m_DC = std::move(DC);
	return TRUE;
}

int CDrawPanel::ExecuteCommand(PCWSTR pszCmd, LPARAM lParam)
{
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
	/* */if (wcsncmp(pszCmd, szCmd_ReSize, cchCmd_ReSize) == 0)
	{
		GetParamList(pszCmd + cchCmd_ReSize, rsTemp, aResult);
		if (aResult.size() != 2)
			return CDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		if (x1 <= 0 || y1 <= 0)
			return CDPE_INVALID_PARAM;
		constexpr int
			c_cxMax = 1000,
			c_cyMax = 1000;
		if (x1 > c_cxMax || y1 > c_cyMax)
			return CDPE_SO_LARGE_SIZE;

		ReSize(x1, y1);
		return CDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_Brush, cchCmd_Brush) == 0)
	{
		GetParamList(pszCmd + cchCmd_Brush, rsTemp, aResult);
		switch (aResult.size())
		{
		case 1u:
		{
			iRet = AnalyzeColorCommand(aResult[0], &argb1);
			if (iRet != CDPE_OK)
				return iRet;
			GpSolidFill* pBrush;
			GdipCreateSolidFill(argb1, &pBrush);
			m_Brushes.push_back(pBrush);
			m_idxCurrBrush = m_Brushes.size() - 1;
		}
		return CDPE_OK;
		case 6u:
		{
			iRet = AnalyzeColorCommand(aResult[4], &argb1);
			if (iRet != CDPE_OK)
				return iRet;
			iRet = AnalyzeColorCommand(aResult[5], &argb2);
			if (iRet != CDPE_OK)
				return iRet;

			GpPoint
				pt1{ _wtoi(aResult[0]),_wtoi(aResult[1]) },
				pt2{ _wtoi(aResult[2]),_wtoi(aResult[3]) };
			GpLineGradient* pLineGradien;
			GdipCreateLineBrushI(&pt1, &pt2, argb1, argb2, Gdiplus::WrapModeTile, &pLineGradien);
			m_Brushes.push_back(pLineGradien);
			m_idxCurrBrush = m_Brushes.size() - 1;
		}
		return CDPE_OK;
		}
		return CDPE_PARAM_COUNT;
	}
	else if (wcsncmp(pszCmd, szCmd_Pen, cchCmd_Pen) == 0)
	{
		GetParamList(pszCmd + cchCmd_Pen, rsTemp, aResult);
		if (aResult.size() != 2)
			return CDPE_PARAM_COUNT;

		cxPen = _wtoi(aResult[1]);
		if (cxPen < 0.f || cxPen>500.f)
			return CDPE_INVALID_PARAM;
		iRet = AnalyzeColorCommand(aResult[0], &argb1);
		if (iRet != CDPE_OK)
			return iRet;
		GpPen* pPen;
		GdipCreatePen1(argb1, (REAL)cxPen, Gdiplus::UnitPixel, &pPen);
		if (!pPen)
			return CDPE_GDIP_ERROR;
		m_Pens.push_back(pPen);
		return CDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_Font, cchCmd_Font) == 0)
	{
		GetParamList(pszCmd + cchCmd_Font, rsTemp, aResult);
		if (aResult.size() != 3)
			return CDPE_PARAM_COUNT;
		GpFontFamily* pFontFamily;
		GdipCreateFontFamilyFromName(aResult[0], NULL, &pFontFamily);
		f1 = (REAL)_wtof(aResult[1]);
		if (f1 > 600.f || f1 < 0.f)
			return CDPE_INVALID_PARAM;
		GpFont* pFont;
		GdipCreateFont(pFontFamily, f1, (Gdiplus::FontStyle)_wtoi(aResult[2]), Gdiplus::UnitPixel, &pFont);
		GdipDeleteFontFamily(pFontFamily);
		if (!pFont)
			return CDPE_GDIP_ERROR;
		m_Fonts.push_back(pFont);
		return CDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_DrawPixel, cchCmd_DrawPixel) == 0)
	{
		GetParamList(pszCmd + cchCmd_DrawPixel, rsTemp, aResult);
		if (aResult.size() != 3)
			return CDPE_PARAM_COUNT;

		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		iRet = AnalyzeColorCommand(aResult[2], &argb1);
		if (iRet != CDPE_OK)
			return iRet;
		if (x1 < 0 || y1 < 0 || x1 >= m_cx || y1 >= m_cy)
			return CDPE_INVALID_PARAM;
		const BYTE byAlpha = eck::GetIntegerByte<3>(argb1);
		Pixel(x1, y1) = eck::ARGBToColorref(argb1) | (byAlpha << 24);
		return CDPE_OK;
	}
	else if (wcsncmp(pszCmd, szCmd_GetImageSize, cchCmd_GetImageSize) == 0)
	{
		auto pCtx = (CDPGETIMAGESIZE*)lParam;
		GpBitmap* pBitmap = CreateBitmapFromCDPBIN(pCtx->Bin);
		if (!pBitmap)
			return CDPE_INVALID_PARAM;
		ResetBitmapDpi(pBitmap);
		GdipGetImageWidth(pBitmap, &pCtx->cx);
		GdipGetImageHeight(pBitmap, &pCtx->cy);
		GdipDisposeImage(pBitmap);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_DrawImage, cchCmd_DrawImage) == 0)
	{
		GetParamList(pszCmd + cchCmd_DrawImage, rsTemp, aResult);
		auto pCtx = (CDPDRAWIMAGE*)lParam;
		GpBitmap* pBitmap;
		BOOL bDel = FALSE;

		switch (pCtx->Img.eMedium)
		{
		case CDPIM_BIN:
			bDel = TRUE;
			pBitmap = CreateBitmapFromCDPBIN(pCtx->Img.Bin);
			break;
		case CDPIM_HBITMAP:
		{
			bDel = TRUE;
			BITMAP bmp;
			GetObjectW(pCtx->Img.hbm, sizeof(bmp), &bmp);
			if (bmp.bmBits && bmp.bmBitsPixel == 32)
				GdipCreateBitmapFromScan0(m_cx, m_cy, bmp.bmWidthBytes, PixelFormat32bppARGB,
					(BYTE*)bmp.bmBits, &pBitmap);
			else
				GdipCreateBitmapFromHBITMAP(pCtx->Img.hbm, NULL, &pBitmap);
		}
		break;

		case CDPIM_GPIMAGE:
			pBitmap = pCtx->Img.pBitmap;
			break;

		default:
			pBitmap = NULL;
			break;
		}
		if (!pBitmap)
			return CDPE_INVALID_PARAM;
		ResetBitmapDpi(pBitmap);

		switch (aResult.size())
		{
		case 2u:
			GdipDrawImageI(m_pGraphics, pBitmap, _wtoi(aResult[0]), _wtoi(aResult[1]));
			break;
		case 4u:
			GdipDrawImageRectI(m_pGraphics, pBitmap,
				_wtoi(aResult[0]), _wtoi(aResult[1]), _wtoi(aResult[2]), _wtoi(aResult[3]));
			break;
		case 8u:
			GdipDrawImageRectRectI(m_pGraphics, pBitmap,
				_wtoi(aResult[0]), _wtoi(aResult[1]), _wtoi(aResult[2]), _wtoi(aResult[3]),
				_wtoi(aResult[4]), _wtoi(aResult[5]), _wtoi(aResult[6]), _wtoi(aResult[7]),
				Gdiplus::UnitPixel, NULL, NULL, NULL);

			break;
		default:
			if (bDel)
				GdipDisposeImage(pBitmap);
			return CDPE_PARAM_COUNT;
		}
		if (bDel)
			GdipDisposeImage(pBitmap);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_GetObjCount, cchCmd_GetObjCount) == 0)
	{
		GetParamList(pszCmd + cchCmd_GetObjCount, rsTemp, aResult);
		if (aResult.size() != 1)
			return CDPE_PARAM_COUNT;
		auto pCtx = (QDPGETOBJCOUNT*)lParam;
		pCtx->cObj = GetObjCount(_wtoi(aResult[0]));
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_Clear, cchCmd_Clear) == 0)
	{
		GetParamList(pszCmd + cchCmd_Clear, rsTemp, aResult);
		switch (aResult.size())
		{
		case 1u:
			iRet = AnalyzeColorCommand(aResult[0], &argb1);
			if (iRet != CDPE_OK)
				return iRet;
			GdipGraphicsClear(m_pGraphics, argb1);
			return CDPE_OK;
		case 0u:
			GdipGraphicsClear(m_pGraphics, 0xFFFFFFFF);
			return CDPE_OK;
		}
		return CDPE_PARAM_COUNT;
		}
	else if (wcsncmp(pszCmd, szCmd_SetTransform, cchCmd_SetTransform) == 0)
	{
		GetParamList(pszCmd + cchCmd_SetTransform, rsTemp, aResult);
		if (aResult.size() != 1)
			return CDPE_PARAM_COUNT;
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
		return CDPE_OK;
		case 0u:
			GdipResetWorldTransform(m_pGraphics);
			return CDPE_OK;
		default:
			return CDPE_PARAM_COUNT;
		}
		}
	else if (wcsncmp(pszCmd, szCmd_MultiplyTransform, cchCmd_MultiplyTransform) == 0)
	{
		GetParamList(pszCmd + cchCmd_MultiplyTransform, rsTemp, aResult);
		if (aResult.size() != 2)
			return CDPE_PARAM_COUNT;
		AnalyzeArrayFloat(aResult[0], aFloat);
		if (aFloat.size() != 6)
			return CDPE_PARAM_COUNT;
		GpMatrix* pMatrix;
		GdipCreateMatrix2(aFloat[0], aFloat[1], aFloat[2], aFloat[3], aFloat[4], aFloat[5], &pMatrix);
		GdipMultiplyWorldTransform(m_pGraphics, pMatrix, (Gdiplus::MatrixOrder)_wtoi(aResult[1]));
		GdipDeleteMatrix(pMatrix);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_SelFont, cchCmd_SelFont) == 0)
	{
		GetParamList(pszCmd + cchCmd_SelFont, rsTemp, aResult);
		if (aResult.size() != 1)
			return CDPE_PARAM_COUNT;
		iRet = _wtoi(aResult[0]);
		if (iRet < 0 || iRet >= (int)m_Fonts.size())
			return CDPE_OUT_OF_RANGE;
		m_idxCurrFont = iRet;
		return CDPE_OK;
		}
		//////////////////////////////////////////矩阵
	else if (wcsncmp(pszCmd, szCmd_MatrixTranslate, cchCmd_MatrixTranslate) == 0)
	{
		auto pCtx = (CDPSTR*)lParam;
		GetParamList(pszCmd + cchCmd_MatrixTranslate, rsTemp, aResult);
		if (aResult.size() != 2)
			return CDPE_PARAM_COUNT;
		GpMatrix* pMatrix;
		GdipCreateMatrix(&pMatrix);
		GdipTranslateMatrix(pMatrix, (REAL)_wtof(aResult[0]), (REAL)_wtof(aResult[1]),
			Gdiplus::MatrixOrderAppend);
		auto rs = GpMatrixToString(pMatrix);
		*pCtx = RefStrToCdpStr(rs);
		GdipDeleteMatrix(pMatrix);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_MatrixShear, cchCmd_MatrixShear) == 0)
	{
		auto pCtx = (CDPSTR*)lParam;
		GetParamList(pszCmd + cchCmd_MatrixShear, rsTemp, aResult);
		GpMatrix* pMatrix;
		switch (aResult.size())
		{
		case 2u:
		{
			GdipCreateMatrix(&pMatrix);
			GdipShearMatrix(pMatrix, (REAL)_wtof(aResult[0]), (REAL)_wtof(aResult[1]), Gdiplus::MatrixOrderAppend);
		}
		break;
		case 4u:
		{
			GdipCreateMatrix(&pMatrix);
			f1 = (float)_wtof(aResult[0]);
			f2 = (float)_wtof(aResult[1]);
			GdipTranslateMatrix(pMatrix, -f1, -f2, Gdiplus::MatrixOrderAppend);
			GdipShearMatrix(pMatrix, (REAL)_wtof(aResult[2]), (REAL)_wtof(aResult[3]), Gdiplus::MatrixOrderAppend);
			GdipTranslateMatrix(pMatrix, f1, f2, Gdiplus::MatrixOrderAppend);
		}
		break;
		default:
			return CDPE_PARAM_COUNT;
		}
		auto rs = GpMatrixToString(pMatrix);
		*pCtx = RefStrToCdpStr(rs);
		GdipDeleteMatrix(pMatrix);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_MatrixRotate, cchCmd_MatrixRotate) == 0)
	{
		auto pCtx = (CDPSTR*)lParam;
		GetParamList(pszCmd + cchCmd_MatrixRotate, rsTemp, aResult);
		GpMatrix* pMatrix;
		switch (aResult.size())
		{
		case 1u:
		{
			GdipCreateMatrix(&pMatrix);
			GdipRotateMatrix(pMatrix, (REAL)_wtof(aResult[0]), Gdiplus::MatrixOrderAppend);
		}
		break;
		case 3u:
		{
			GdipCreateMatrix(&pMatrix);
			f1 = (float)_wtof(aResult[0]);
			f2 = (float)_wtof(aResult[1]);
			GdipTranslateMatrix(pMatrix, -f1, -f2, Gdiplus::MatrixOrderAppend);
			GdipRotateMatrix(pMatrix, (REAL)_wtof(aResult[2]), Gdiplus::MatrixOrderAppend);
			GdipTranslateMatrix(pMatrix, f1, f2, Gdiplus::MatrixOrderAppend);
		}
		break;
		default:
			return CDPE_PARAM_COUNT;
		}

		auto rs = GpMatrixToString(pMatrix);
		*pCtx = RefStrToCdpStr(rs);
		GdipDeleteMatrix(pMatrix);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_MatrixScale, cchCmd_MatrixScale) == 0)
	{
		auto pCtx = (CDPSTR*)lParam;
		GetParamList(pszCmd + cchCmd_MatrixScale, rsTemp, aResult);
		GpMatrix* pMatrix;
		switch (aResult.size())
		{
		case 2u:
		{
			GdipCreateMatrix(&pMatrix);
			GdipScaleMatrix(pMatrix, (REAL)_wtof(aResult[0]), (REAL)_wtof(aResult[1]), Gdiplus::MatrixOrderAppend);
		}
		break;
		case 4u:
		{
			GdipCreateMatrix(&pMatrix);
			f1 = (float)_wtof(aResult[0]);
			f2 = (float)_wtof(aResult[1]);
			GdipTranslateMatrix(pMatrix, -f1, -f2, Gdiplus::MatrixOrderAppend);
			GdipScaleMatrix(pMatrix, (REAL)_wtof(aResult[2]), (REAL)_wtof(aResult[3]), Gdiplus::MatrixOrderAppend);
			GdipTranslateMatrix(pMatrix, f1, f2, Gdiplus::MatrixOrderAppend);
		}
		break;
		default:
			return CDPE_PARAM_COUNT;
		}

		auto rs = GpMatrixToString(pMatrix);
		*pCtx = RefStrToCdpStr(rs);
		GdipDeleteMatrix(pMatrix);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_MatrixInvert, cchCmd_MatrixInvert) == 0)
	{
		auto pCtx = (CDPSTR*)lParam;
		GetParamList(pszCmd + cchCmd_MatrixInvert, rsTemp, aResult);
		if (aResult.size() != 1)
			return CDPE_PARAM_COUNT;
		AnalyzeArrayFloat(aResult[0], aFloat);
		if (aFloat.size() != 6)
			return CDPE_PARAM_COUNT;
		GpMatrix* pMatrix;
		GdipCreateMatrix2(aFloat[0], aFloat[1], aFloat[2], aFloat[3], aFloat[4], aFloat[5], &pMatrix);
		GdipInvertMatrix(pMatrix);

		auto rs = GpMatrixToString(pMatrix);
		*pCtx = RefStrToCdpStr(rs);
		GdipDeleteMatrix(pMatrix);
		return CDPE_OK;
		}
		//////////////////////////////////////////下面的指令需要画笔
	else if (wcsncmp(pszCmd, szCmd_ListPen, cchCmd_ListPen) == 0)
	{
		auto pCtx = (QDPLISTOBJ*)lParam;
		GetParamList(pszCmd + cchCmd_ListPen, rsTemp, aResult);
		argb1 = 0xFFFFFFFF;
		if (aResult.size() == 1)
		{
			iRet = AnalyzeColorCommand(aResult[0], &argb1);
			if (iRet != CDPE_OK)
				return iRet;
		}
		else if (aResult.size() != 0)
			return CDPE_INVALID_PARAM;
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
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_DelPen, cchCmd_DelPen) == 0)
	{
		GetParamList(pszCmd + cchCmd_DelPen, rsTemp, aResult);
		switch (aResult.size())
		{
		case 1u:
			iRet = _wtoi(aResult[0]);
			if (iRet < 0 || iRet >= (int)m_Pens.size())
				return CDPE_OUT_OF_RANGE;
			GdipDeletePen(m_Pens[iRet]);
			m_Pens.erase(m_Pens.begin() + iRet);
			return CDPE_OK;
		case 0u:
			for (auto x : m_Pens)
				GdipDeletePen(x);
			m_Pens.clear();
			m_idxCurrPen = -1;
			return CDPE_OK;
		}
		return CDPE_PARAM_COUNT;
		}
	else if (wcsncmp(pszCmd, szCmd_SelPen, cchCmd_SelPen) == 0)
	{
		GetParamList(pszCmd + cchCmd_SelPen, rsTemp, aResult);
		if (aResult.size() != 1)
			return CDPE_PARAM_COUNT;
		iRet = _wtoi(aResult[0]);
		if (iRet < 0 || iRet >= (int)m_Pens.size())
			return CDPE_OUT_OF_RANGE;
		m_idxCurrPen = iRet;
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_DrawLine, cchCmd_DrawLine) == 0)// .DrawLine(x1,y1,x2,y2,clr,width)
	{
		if (m_idxCurrPen >= m_Pens.size())
			return CDPE_NO_PEN;
		GetParamList(pszCmd + cchCmd_DrawLine, rsTemp, aResult);
		if (aResult.size() != 4)
			return CDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);

		GdipDrawLineI(m_pGraphics, m_Pens[m_idxCurrPen], x1, y1, x2, y2);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_DrawPolyLine, cchCmd_DrawPolyLine) == 0)
	{
		if (m_idxCurrPen >= m_Pens.size())
			return CDPE_NO_PEN;
		GetParamList(pszCmd + cchCmd_DrawPolyLine, rsTemp, aResult);
		if (aResult.size() != 1)
			return CDPE_PARAM_COUNT;
		AnalyzeArrayPoint(aResult[0], aPoints);
		if (!aPoints.size())
			return CDPE_NULL_ARRAY;
		auto i = GdipDrawLinesI(m_pGraphics, m_Pens[m_idxCurrPen], aPoints.data(), (int)aPoints.size());
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_DrawRect, cchCmd_DrawRect) == 0)
	{
		if (m_idxCurrPen >= m_Pens.size())
			return CDPE_NO_PEN;
		GetParamList(pszCmd + cchCmd_DrawRect, rsTemp, aResult);
		if (aResult.size() != 4)
			return CDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);

		GdipDrawRectangleI(m_pGraphics, m_Pens[m_idxCurrPen], x1, y1, x2, y2);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_DrawArc, cchCmd_DrawArc) == 0)
	{
		if (m_idxCurrPen >= m_Pens.size())
			return CDPE_NO_PEN;
		GetParamList(pszCmd + cchCmd_DrawArc, rsTemp, aResult);
		if (aResult.size() != 6)
			return CDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);

		f1 = (float)_wtof(aResult[4]);
		f2 = (float)_wtof(aResult[5]);

		GdipDrawArcI(m_pGraphics, m_Pens[m_idxCurrPen], x1, y1, x2, y2, f1, f2);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_DrawEllipse, cchCmd_DrawEllipse) == 0)
	{
		if (m_idxCurrPen >= m_Pens.size())
			return CDPE_NO_PEN;
		GetParamList(pszCmd + cchCmd_DrawEllipse, rsTemp, aResult);
		if (aResult.size() != 4)
			return CDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);

		GdipDrawEllipseI(m_pGraphics, m_Pens[m_idxCurrPen], x1, y1, x2, y2);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_DrawPie, cchCmd_DrawPie) == 0)
	{
		if (m_idxCurrPen >= m_Pens.size())
			return CDPE_NO_PEN;
		GetParamList(pszCmd + cchCmd_DrawPie, rsTemp, aResult);
		if (aResult.size() != 6)
			return CDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);

		f1 = (float)_wtof(aResult[4]);
		f2 = (float)_wtof(aResult[5]);

		GdipDrawPieI(m_pGraphics, m_Pens[m_idxCurrPen], x1, y1, x2, y2, f1, f2);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_DrawPolygon, cchCmd_DrawPolygon) == 0)
	{
		if (m_idxCurrPen >= m_Pens.size())
			return CDPE_NO_PEN;
		GetParamList(pszCmd + cchCmd_DrawPolygon, rsTemp, aResult);
		if (aResult.size() != 1)
			return CDPE_PARAM_COUNT;
		AnalyzeArrayPoint(aResult[0], aPoints);
		if (!aPoints.size())
			return CDPE_NULL_ARRAY;
		GdipDrawPolygonI(m_pGraphics, m_Pens[m_idxCurrPen], aPoints.data(), (int)aPoints.size());
		return CDPE_OK;
		}
		//////////////////////////////////////////下面的指令需要画刷
	else if (wcsncmp(pszCmd, szCmd_ListBrush, cchCmd_ListBrush) == 0)
	{
		auto pCtx = (QDPLISTOBJ*)lParam;
		GetParamList(pszCmd + cchCmd_ListBrush, rsTemp, aResult);
		argb1 = 0xFFFFFFFF;
		if (aResult.size() == 1)
		{
			iRet = AnalyzeColorCommand(aResult[0], &argb1);
			if (iRet != CDPE_OK)
				return iRet;
		}
		else if (aResult.size() != 0)
			return CDPE_INVALID_PARAM;
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
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_DelBrush, cchCmd_DelBrush) == 0)
	{
		GetParamList(pszCmd + cchCmd_DelBrush, rsTemp, aResult);
		switch (aResult.size())
		{
		case 1u:
			iRet = _wtoi(aResult[0]);
			if (iRet < 0 || iRet >= (int)m_Brushes.size())
				return CDPE_OUT_OF_RANGE;
			GdipDeleteBrush(m_Brushes[iRet]);
			m_Brushes.erase(m_Brushes.begin() + iRet);
			return CDPE_OK;
		case 0u:
			for (auto x : m_Brushes)
				GdipDeleteBrush(x);
			m_Brushes.clear();
			m_idxCurrBrush = -1;
			return CDPE_OK;
		}
		return CDPE_PARAM_COUNT;
		}
	else if (wcsncmp(pszCmd, szCmd_SelBrush, cchCmd_SelBrush) == 0)
	{
		GetParamList(pszCmd + cchCmd_SelBrush, rsTemp, aResult);
		if (aResult.size() != 1)
			return CDPE_PARAM_COUNT;
		iRet = _wtoi(aResult[0]);
		if (iRet < 0 || iRet >= (int)m_Brushes.size())
			return CDPE_OUT_OF_RANGE;
		m_idxCurrBrush = iRet;
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_FillRect, cchCmd_FillRect) == 0)
	{
		if (m_idxCurrBrush >= m_Brushes.size())
			return CDPE_NO_BRUSH;
		GetParamList(pszCmd + cchCmd_FillRect, rsTemp, aResult);
		if (aResult.size() != 4)
			return CDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);

		GdipFillRectangleI(m_pGraphics, m_Brushes[m_idxCurrBrush], x1, y1, x2, y2);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_FillEllipse, cchCmd_FillEllipse) == 0)
	{
		if (m_idxCurrBrush >= m_Brushes.size())
			return CDPE_NO_BRUSH;
		GetParamList(pszCmd + cchCmd_FillEllipse, rsTemp, aResult);
		if (aResult.size() != 4)
			return CDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);

		GdipFillEllipseI(m_pGraphics, m_Brushes[m_idxCurrBrush], x1, y1, x2, y2);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_FillPie, cchCmd_FillPie) == 0)
	{
		if (m_idxCurrBrush >= m_Brushes.size())
			return CDPE_NO_BRUSH;
		GetParamList(pszCmd + cchCmd_FillPie, rsTemp, aResult);
		if (aResult.size() != 6)
			return CDPE_PARAM_COUNT;
		x1 = _wtoi(aResult[0]);
		y1 = _wtoi(aResult[1]);
		x2 = _wtoi(aResult[2]);
		y2 = _wtoi(aResult[3]);

		f1 = (float)_wtof(aResult[4]);
		f2 = (float)_wtof(aResult[5]);

		GdipFillPieI(m_pGraphics, m_Brushes[m_idxCurrBrush], x1, y1, x2, y2, f1, f2);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_FillPolygon, cchCmd_FillPolygon) == 0)
	{
		if (m_idxCurrBrush >= m_Brushes.size())
			return CDPE_NO_BRUSH;
		GetParamList(pszCmd + cchCmd_FillPolygon, rsTemp, aResult);
		if (aResult.size() != 1)
			return CDPE_PARAM_COUNT;
		AnalyzeArrayPoint(aResult[0], aPoints);
		if (!aPoints.size())
			return CDPE_NULL_ARRAY;

		std::vector<BYTE> aTypes(aPoints.size(), Gdiplus::PathPointTypeLine);
		GpPath* pPath;
		GdipCreatePath2I(aPoints.data(), aTypes.data(), (int)aPoints.size(), Gdiplus::FillModeWinding, &pPath);
		GdipFillPath(m_pGraphics, m_Brushes[m_idxCurrBrush], pPath);
		GdipDeletePath(pPath);
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_FillText, cchCmd_FillText) == 0)
	{
		if (m_idxCurrBrush >= m_Brushes.size())
			return CDPE_NO_BRUSH;
		if (m_idxCurrFont >= m_Fonts.size())
			return CDPE_NO_FONT;
		GetParamList(pszCmd + cchCmd_FillText, rsTemp, aResult);
		if (aResult.size() != 7)
			return CDPE_PARAM_COUNT;
		PCWSTR pszText = (PCWSTR)lParam;
		GpRectF rcF{ (REAL)_wtof(aResult[0]),(REAL)_wtof(aResult[1]),(REAL)_wtof(aResult[2]),(REAL)_wtof(aResult[3]) };
		GpStringFormat* pStringFormat;
		GdipCreateStringFormat(
			ToBool(aResult[4]) ? (Gdiplus::StringFormatFlags)0 : Gdiplus::StringFormatFlagsNoWrap,
			LANG_NEUTRAL, &pStringFormat);
		GdipSetStringFormatAlign(pStringFormat, (Gdiplus::StringAlignment)_wtoi(aResult[5]));
		GdipSetStringFormatLineAlign(pStringFormat, (Gdiplus::StringAlignment)_wtoi(aResult[6]));

		GdipDrawString(m_pGraphics, pszText, -1, m_Fonts[m_idxCurrFont],
			&rcF, pStringFormat, m_Brushes[m_idxCurrBrush]);
		GdipDeleteStringFormat(pStringFormat);
		return CDPE_OK;
		}
		//////////////////////////////////////////下面的指令需要字体
	else if (wcsncmp(pszCmd, szCmd_ListFont, cchCmd_ListFont) == 0)
	{
		auto pCtx = (QDPLISTOBJ*)lParam;

		constexpr int
			c_cyPadding = 12,
			c_cxPadding = 10;
		constexpr PCWSTR c_pszTest = L"字体 Font 123 ";
		int cy = (((int)m_Brushes.size()) + 1) * c_cyPadding;

		GpRectF rcF{ 0,0,1000.f,500.f }, rcF2;

		GpStringFormat* pStringFormat;
		GdipCreateStringFormat(Gdiplus::StringFormatFlagsNoClip | Gdiplus::StringFormatFlagsMeasureTrailingSpaces, LANG_NEUTRAL, &pStringFormat);
		GdipSetStringFormatLineAlign(pStringFormat, Gdiplus::StringAlignmentCenter);

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
			rcF.Y = (REAL)y;
			rcF.Height = aHeight[i];
			GdipDrawString(pGraphics, c_pszTest, -1, m_Fonts[i], &rcF, pStringFormat, pBrush);

			y += ((int)rcF.Height + c_cyPadding);
		}
		GdipDeleteStringFormat(pStringFormat);
		GdipDeleteBrush(pBrush);
		GdipDeleteGraphics(pGraphics);
		pCtx->pBitmap = pBitmap;
		return CDPE_OK;
		}
	else if (wcsncmp(pszCmd, szCmd_DelFont, cchCmd_DelFont) == 0)
	{
		GetParamList(pszCmd + cchCmd_DelFont, rsTemp, aResult);
		switch (aResult.size())
		{
		case 1u:
			iRet = _wtoi(aResult[0]);
			if (iRet < 0 || iRet >= (int)m_Fonts.size())
				return CDPE_OUT_OF_RANGE;
			GdipDeleteFont(m_Fonts[iRet]);
			m_Fonts.erase(m_Fonts.begin() + iRet);
			return CDPE_OK;
		case 0u:
			for (auto x : m_Fonts)
				GdipDeleteFont(x);
			m_Fonts.clear();
			m_idxCurrFont = -1;
			return CDPE_OK;
		}
		return CDPE_PARAM_COUNT;
		}
	else if (wcsncmp(pszCmd, szCmd_GetStringSize, cchCmd_GetStringSize) == 0)
	{
		auto pCtx = (QDPGETSTRINGSIZE*)lParam;
		if (m_idxCurrFont >= m_Fonts.size())
			return CDPE_NO_FONT;
		GetParamList(pszCmd + cchCmd_GetStringSize, rsTemp, aResult);
		if (aResult.size() != 7)
			return CDPE_PARAM_COUNT;

		GpRectF rcF{ (REAL)_wtof(aResult[0]),(REAL)_wtof(aResult[1]),(REAL)_wtof(aResult[2]),(REAL)_wtof(aResult[3]) }, rcF2;
		GpStringFormat* pStringFormat;
		GdipCreateStringFormat(ToBool(aResult[4]) ? (Gdiplus::StringFormatFlags)0 : Gdiplus::StringFormatFlagsNoWrap,
			LANG_NEUTRAL, &pStringFormat);
		GdipSetStringFormatAlign(pStringFormat, (Gdiplus::StringAlignment)_wtoi(aResult[5]));
		GdipSetStringFormatLineAlign(pStringFormat, (Gdiplus::StringAlignment)_wtoi(aResult[6]));

		GdipMeasureString(m_pGraphics, pCtx->pszText, -1, m_Fonts[m_idxCurrFont],
			&rcF, pStringFormat, &rcF2, &pCtx->cchFitted, &pCtx->cLinesFilled);
		pCtx->cx = (int)rcF2.Width;
		pCtx->cy = (int)rcF2.Height;
		GdipDeleteStringFormat(pStringFormat);
		}
		return CDPE_INVALID_CMD;
}

int CDrawPanel::ExecuteCommand(PCWSTR pszCmd, const CDPIMAGE* pImgIn,
	CDPSTR& sDisplay, CDPIMAGE* pImgOut, CDPImageMedium eMedium)
{
	int i;
	if (wcsncmp(pszCmd, szCmd_DrawImage, cchCmd_DrawImage) == 0)
	{
		CDPDRAWIMAGE img{ *pImgIn };
		if ((i = ExecuteCommand(pszCmd, (LPARAM)&img)) == CDPE_OK)
			return i;
	}
	else if (wcsncmp(pszCmd, szCmd_GetImageSize, cchCmd_GetImageSize) == 0)
	{
		if (pImgIn->eMedium != CDPIM_BIN)
		{
			i = CDPE_INVALID_MEDIUM;
			goto MakeErrText;
		}
		CDPGETIMAGESIZE img{ pImgIn->Bin };
		if ((i = ExecuteCommand(pszCmd, (LPARAM)&img)) == CDPE_OK)
		{
			auto rs = eck::Format(L"宽度：%u，高度：%u", img.cx, img.cy);
			sDisplay = RefStrToCdpStr(rs);
			return i;
		}
	}
	else if (wcsncmp(pszCmd, szCmd_MatrixTranslate, cchCmd_MatrixTranslate) == 0
		|| wcsncmp(pszCmd, szCmd_MatrixShear, cchCmd_MatrixShear) == 0
		|| wcsncmp(pszCmd, szCmd_MatrixRotate, cchCmd_MatrixRotate) == 0
		|| wcsncmp(pszCmd, szCmd_MatrixScale, cchCmd_MatrixScale) == 0
		|| wcsncmp(pszCmd, szCmd_MatrixInvert, cchCmd_MatrixInvert) == 0)
	{
		if ((i = ExecuteCommand(pszCmd, (LPARAM)&sDisplay)) == CDPE_OK)
			return i;
	}
	else if (wcsncmp(pszCmd, szCmd_FillText, cchCmd_FillText) == 0)
	{
		const auto pszRBracket = wcsstr(pszCmd + cchCmd_FillText, L")");
		if (!pszRBracket)
		{
			i = CDPE_INVALID_CMD;
			goto MakeErrText;
		}
		if (*(pszRBracket + 1) == L'\0')
		{
			i = CDPE_INVALID_PARAM;
			goto MakeErrText;
		}
		if ((i = ExecuteCommand(pszCmd, (LPARAM)(pszRBracket + 1))) == CDPE_OK)
			return i;
	}
	else
	{
		if ((i = ExecuteCommand(pszCmd, 0) == CDPE_OK))
			return i;
	}

MakeErrText:
	auto rsErr = eck::Format(L"执行命令时出错：%s(%d)", CdpGetErrInfo(i), i);
	sDisplay = RefStrToCdpStr(rsErr);
	return i;
}
