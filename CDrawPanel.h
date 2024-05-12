#pragma once
#include "pch.h"
#include "CDPDefine.h"

struct DpStr
{
	BOOL bRefStr;
	union
	{
		eck::CRefStrW rs;
		CDPSTR str;
		BYTE byDummy[std::max(sizeof(rs), sizeof(str))];
	};

	DpStr() :bRefStr{ FALSE }, byDummy{}
	{
	}

	~DpStr()
	{
		if (bRefStr)
			rs.~CRefStrT();
		else
			if (str.cchAlloc && str.psz)
				eck::TRefStrDefAlloc<WCHAR>{}.deallocate(str.psz, str.cchAlloc);
	}

	ECK_DISABLE_COPY_MOVE(DpStr);

	EckInline void From(eck::CRefStrW& rs)
	{
		if (bRefStr)
			rs = std::move(rs);
		else
			str.psz = rs.Detach(str.cchAlloc, str.cch);
	}

	EckInline void From(const CDPSTR& s)
	{
		if (bRefStr)
			rs.Attach(s.psz, s.cchAlloc, s.cch);
		else
			str = s;
	}

	EckInline void Clear()
	{
		if (bRefStr)
			rs.Clear();
		else
		{
			if (str.cchAlloc && str.psz)
				eck::TRefStrDefAlloc<WCHAR>{}.deallocate(str.psz, str.cchAlloc);
			str = {};
		}
	}
};

class CDrawPanel
{
private:
	GpGraphics* m_pGraphics{};
	eck::CEzCDC m_DC{};

	std::vector<GpBrush*> m_Brushes{};
	std::vector<GpPen*> m_Pens{};
	std::vector<GpFont*> m_Fonts{};

	size_t m_idxCurrBrush{ (size_t)-1 };
	size_t m_idxCurrPen{ (size_t)-1 };
	size_t m_idxCurrFont{ (size_t)-1 };

	int m_cx{}, m_cy{};

	BITMAP m_Bmp{};

	void ResetBitmapDpi(GpBitmap* pBitmap)
	{
		REAL xDpi, yDpi;
		GdipGetDpiX(m_pGraphics, &xDpi);
		GdipGetDpiY(m_pGraphics, &yDpi);
		GdipBitmapSetResolution(pBitmap, xDpi, yDpi);
	}

	EckInline constexpr DWORD& Pixel(int x, int y)
	{
		return *(DWORD*)((BYTE*)m_Bmp.bmBits + m_Bmp.bmWidthBytes * y + x);
	}
public:
	ECK_DISABLE_COPY_MOVE_DEF_CONS(CDrawPanel)
public:
	~CDrawPanel()
	{
		UnInit();
	}

	BOOL Init(int cx, int cy);

	BOOL UnInit();

	BOOL ReSize(int cx, int cy);

	int ExecuteCommand(PCWSTR pszCmd, LPARAM lParam);

	EckInline size_t GetObjCount(int i)
	{
		switch (i)
		{
		case CDPOT_PEN: return m_Pens.size();
		case CDPOT_BRUSH: return m_Brushes.size();
		case CDPOT_FONT: return m_Fonts.size();
		}
		return (size_t)-1;
	}

	CDPBIN GetImageBin(int i)
	{
		if (i < CDPIT_BEGIN_ || i >= CDPIT_END_)
			return {};
		eck::ImageType Type[]{ eck::ImageType::Bmp,eck::ImageType::Png,eck::ImageType::Jpeg };
		GpBitmap* pBitmap;
		GdipCreateBitmapFromScan0(m_cx, m_cy, m_Bmp.bmWidthBytes, PixelFormat32bppARGB,
			(BYTE*)m_Bmp.bmBits, &pBitmap);
		auto rb = eck::SaveGpImage(pBitmap, Type[i]);
		CDPBIN bin;
		bin.pData = rb.Detach(bin.cbAlloc, bin.cb);
		return bin;
	}

	HBITMAP CloneImageToHBITMAP()
	{
		eck::CDib dib{};
		dib.Create(m_cx, -m_cy);
		BITMAP bmp;
		GetObjectW(dib.GetHBitmap(), sizeof(bmp), &bmp);
		memcpy(bmp.bmBits, m_Bmp.bmBits, m_Bmp.bmWidthBytes * m_Bmp.bmHeight);
		return dib.Detach();
	}

	GpBitmap* CloneImageToGpBitmap()
	{
		GpBitmap* pBitmap{};
		GdipCreateBitmapFromScan0(m_cx, m_cy, m_Bmp.bmWidthBytes, PixelFormat32bppARGB,
			(BYTE*)m_Bmp.bmBits, &pBitmap);
		return pBitmap;
	}

	int ExecuteCommand(PCWSTR pszCmd, const CDPIMAGE* pImgIn,
		CDPSTR& sDisplay, CDPIMAGE* pImgOut, CDPImageMedium eMedium);
};

#define CDPDECLCMD(CmdName)						\
		constexpr static WCHAR szCmd_##CmdName[]{		\
							L"."				\
							ECKTOSTRW(CmdName)	\
							L"("  };			\
		constexpr static int cchCmd_##CmdName = (int)ARRAYSIZE(szCmd_##CmdName) - 1;