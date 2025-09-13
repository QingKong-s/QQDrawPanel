/*
* Command DrawPanel
*
* CDPDefine.h : SDK头文件
*
* Copyright(C) 2024 QingKong
*/
#pragma once
#include <Windows.h>
#ifdef CDP_DBG_LIB
#include <eck\CRefBin.h>
#endif

class CDrawPanel;

enum CDPObjType
{
	CDPOT_PEN,
	CDPOT_BRUSH,
	CDPOT_FONT,
};

enum CDPError
{
	CDPE_BEGIN_,
	CDPE_OK = CDPE_BEGIN_,
	CDPE_PARAM_COUNT,
	CDPE_INVALID_PARAM,
	CDPE_NO_PEN,
	CDPE_NO_BRUSH,
	CDPE_OUT_OF_RANGE,
	CDPE_NULL_ARRAY,
	CDPE_INVALID_CMD,
	CDPE_NO_FONT,
	CDPE_SO_LARGE_SIZE,
	CDPE_GDIP_ERROR,
	CDPE_INVALID_MEDIUM,
	CDPE_INVALID_IMAGE,
	CDPE_URL_REQUEST_FAILED,

	CDPE_END_,
};

enum CDPImageType
{
	CDPIT_BEGIN_,
	CDPIT_BMP = CDPIT_BEGIN_,
	CDPIT_PNG,
	CDPIT_JPG,

	CDPIT_END_
};

enum CDPImageMedium
{
	CDPIM_NONE = -1,
	CDPIM_BIN,
	CDPIM_HBITMAP,
	CDPIM_GPIMAGE
};

constexpr inline PCWSTR c_pszErrInfo[]
{
	L"操作成功完成",
	L"参数数目不正确",
	L"无效参数",
	L"无画笔",
	L"无画刷",
	L"下标超界",
	L"空数组",
	L"无效指令",
	L"无字体",
	L"给定的图面尺寸太大",
	L"URL请求失败",
};

__forceinline PCWSTR CdpGetErrInfo(int i)
{
	if (i < CDPE_BEGIN_ || i >= CDPE_END_)
		return L"";
	else
		return c_pszErrInfo[i];
}

struct CDPBIN
{
	void* pData;
	size_t cb;
	size_t cbAlloc;
};

struct CDPSTR
{
	PWSTR psz;
	int cch;
	int cchAlloc;
};

struct CDPIMAGE
{
	CDPImageMedium eMedium;
	union
	{
		struct
		{
			CDPImageType eType;
			CDPBIN Bin;
		};
		HBITMAP hbm;
		GpBitmap* pBitmap;
	};
};

struct CDPGETIMAGESIZE
{
	CDPBIN Bin;
	UINT cx;
	UINT cy;
};

struct CDPDRAWIMAGE
{
	CDPIMAGE Img;
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
	CDPBIN Bin;
	GpBitmap* pBitmap;
	HBITMAP hBitmap;
};