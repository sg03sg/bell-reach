#pragma once

#include <string>
#include <vector>

//
// 문자열 한 줄을 흰 글자의 알파 비트맵으로 그린다.
//
// 한글은 글자 수가 많아 글꼴 아틀라스를 미리 굽기 어렵고, Core Profile에서는
// glutBitmapCharacter 같은 옛 함수도 쓸 수 없다. 그래서 운영체제의 글꼴 엔진에
// 한 줄을 통째로 맡긴다. 한글 조합, 자간, 없는 글자의 대체 글꼴까지 OS가 처리한다.
//
//   macOS   : CoreText  (Apple SD Gothic Neo)
//   Windows : GDI       (맑은 고딕)
//   그 외   : 지원하지 않음 — false를 돌려주고, 글자는 그려지지 않는다
//
// 결과 비트맵은 0행이 이미지의 맨 위다. 한 픽셀이 1바이트(0 = 빈 곳, 255 = 글자)다.
//
bool RasterizeTextLine(const std::string& utf8, float pixelSize,
                       std::vector<unsigned char>& outAlpha, int& outWidth, int& outHeight);
