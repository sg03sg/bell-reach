#include "stdafx.h"
#include "TextRaster.h"

#include <cmath>

// 글자 가장자리가 잘리지 않도록 사방에 두는 여백(px)
static const int kPadding = 2;

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

bool RasterizeTextLine(const std::string& utf8, float pixelSize,
                       std::vector<unsigned char>& outAlpha, int& outWidth, int& outHeight)
{
	if (utf8.empty())
	{
		return false;
	}

	CFStringRef text = CFStringCreateWithBytes(kCFAllocatorDefault,
	                                           reinterpret_cast<const UInt8*>(utf8.data()),
	                                           static_cast<CFIndex>(utf8.size()),
	                                           kCFStringEncodingUTF8, false);
	if (text == NULL)
	{
		return false;
	}

	// 없는 글꼴이면 CoreText가 비슷한 글꼴로 대신하고, 한 줄 안에서
	// 글자별로 대체 글꼴을 찾아 주므로 한글이 네모로 깨지지 않는다.
	CTFontRef font = CTFontCreateWithName(CFSTR("AppleSDGothicNeo-Medium"), pixelSize, NULL);

	// 글자색은 그리는 쪽(비트맵 컨텍스트)의 채우기 색을 따른다.
	CFStringRef keys[2] = { kCTFontAttributeName, kCTForegroundColorFromContextAttributeName };
	CFTypeRef values[2] = { font, kCFBooleanTrue };
	CFDictionaryRef attributes = CFDictionaryCreate(kCFAllocatorDefault,
	                                                reinterpret_cast<const void**>(keys),
	                                                reinterpret_cast<const void**>(values), 2,
	                                                &kCFTypeDictionaryKeyCallBacks,
	                                                &kCFTypeDictionaryValueCallBacks);
	CFAttributedStringRef attributed = CFAttributedStringCreate(kCFAllocatorDefault, text, attributes);
	CTLineRef line = CTLineCreateWithAttributedString(attributed);

	CGFloat ascent = 0.0;
	CGFloat descent = 0.0;
	CGFloat leading = 0.0;
	const double lineWidth = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);

	outWidth = static_cast<int>(std::ceil(lineWidth)) + kPadding * 2;
	outHeight = static_cast<int>(std::ceil(ascent + descent)) + kPadding * 2;
	outAlpha.assign(static_cast<size_t>(outWidth) * outHeight, 0);

	// 회색조 비트맵 하나에 흰색으로 그리면, 밝기가 곧 글자의 덮임 정도(알파)가 된다.
	CGColorSpaceRef gray = CGColorSpaceCreateDeviceGray();
	CGContextRef context = CGBitmapContextCreate(outAlpha.data(), outWidth, outHeight, 8, outWidth,
	                                             gray, kCGImageAlphaNone);
	bool drawn = false;
	if (context != NULL)
	{
		CGContextSetAllowsAntialiasing(context, true);
		CGContextSetShouldAntialias(context, true);
		CGContextSetGrayFillColor(context, 1.0, 1.0);

		// CoreGraphics는 왼쪽 아래가 원점이다. 기준선을 아래 여백 + 내림 높이에 둔다.
		CGContextSetTextPosition(context, kPadding, kPadding + descent);
		CTLineDraw(line, context);

		CGContextRelease(context);
		drawn = true;
	}

	CGColorSpaceRelease(gray);
	CFRelease(line);
	CFRelease(attributed);
	CFRelease(attributes);
	CFRelease(font);
	CFRelease(text);

	return drawn && outWidth > kPadding * 2;
}

#elif defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

bool RasterizeTextLine(const std::string& utf8, float pixelSize,
                       std::vector<unsigned char>& outAlpha, int& outWidth, int& outHeight)
{
	if (utf8.empty())
	{
		return false;
	}

	const int wideLength = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), NULL, 0);
	if (wideLength <= 0)
	{
		return false;
	}
	std::wstring wide(static_cast<size_t>(wideLength), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &wide[0], wideLength);

	HDC dc = CreateCompatibleDC(NULL);
	if (dc == NULL)
	{
		return false;
	}

	// 음수 높이는 "글자 크기"를 뜻한다. ClearType 대신 회색조 안티앨리어싱을 써야
	// 색 번짐 없이 한 채널을 알파로 쓸 수 있다.
	HFONT font = CreateFontW(-static_cast<int>(pixelSize + 0.5f), 0, 0, 0, FW_MEDIUM,
	                         FALSE, FALSE, FALSE, HANGUL_CHARSET,
	                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
	                         DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");
	HGDIOBJ previousFont = SelectObject(dc, font);

	SIZE extent = { 0, 0 };
	GetTextExtentPoint32W(dc, wide.c_str(), wideLength, &extent);

	outWidth = extent.cx + kPadding * 2;
	outHeight = extent.cy + kPadding * 2;

	BITMAPINFO info;
	ZeroMemory(&info, sizeof(info));
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = outWidth;
	info.bmiHeader.biHeight = -outHeight;   // 음수 = 위에서 아래로 저장
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;

	void* bits = NULL;
	HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, NULL, 0);
	bool drawn = false;

	if (bitmap != NULL && bits != NULL)
	{
		HGDIOBJ previousBitmap = SelectObject(dc, bitmap);

		// 검은 바탕에 흰 글자. 초록 채널을 알파로 옮긴다.
		SetBkMode(dc, OPAQUE);
		SetBkColor(dc, RGB(0, 0, 0));
		SetTextColor(dc, RGB(255, 255, 255));
		TextOutW(dc, kPadding, kPadding, wide.c_str(), wideLength);

		outAlpha.assign(static_cast<size_t>(outWidth) * outHeight, 0);
		const unsigned char* pixels = static_cast<const unsigned char*>(bits);
		for (int i = 0; i < outWidth * outHeight; ++i)
		{
			outAlpha[i] = pixels[i * 4 + 1];
		}

		SelectObject(dc, previousBitmap);
		DeleteObject(bitmap);
		drawn = true;
	}

	SelectObject(dc, previousFont);
	DeleteObject(font);
	DeleteDC(dc);

	return drawn && outWidth > kPadding * 2;
}

#else

bool RasterizeTextLine(const std::string&, float, std::vector<unsigned char>&, int&, int&)
{
	return false;
}

#endif
