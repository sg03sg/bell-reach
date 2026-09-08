# SimpleGame — macOS 빌드 가이드

Visual Studio 프로젝트(`SimpleGame.sln`)는 그대로 두고, macOS에서도 동일한 소스를
빌드할 수 있도록 설정을 추가했습니다. Windows 빌드는 영향을 받지 않습니다.

## 필요한 것

Xcode Command Line Tools 뿐입니다. (OpenGL / GLUT는 macOS 시스템 프레임워크 사용)

```bash
xcode-select --install
```

## 빌드 & 실행

```bash
make run
```

| 명령 | 설명 |
| --- | --- |
| `make` | Debug 빌드 (`build/SimpleGame`) |
| `make run` | 빌드 후 실행 |
| `make release` | 최적화 빌드 |
| `make clean` | 빌드 산출물 삭제 |

셰이더는 실행 파일 옆(`build/Shaders`)으로 자동 복사되므로 `build` 디렉터리에서
실행해야 합니다. `make run` 이 이를 처리합니다.

## VS Code

`.vscode/` 설정이 포함되어 있습니다.

- `⌘⇧B` — 빌드
- `F5` — lldb 디버깅 (빌드 후 `build` 디렉터리에서 실행)
- 추천 확장: **C/C++ Extension Pack**, **Shader languages support**

## Windows와 달라진 점

| 항목 | 내용 |
| --- | --- |
| 헤더 include | `"Dependencies\glew.h"` → `"Dependencies/GL_Platform.h"` (역슬래시는 clang에서 인식 불가) |
| GL 로더 | macOS는 `OpenGL.framework`가 코어 함수를 직접 제공 → GLEW 불필요. `GL_Platform.h`가 `glewInit()` / `glewIsSupported()`를 무해한 shim으로 대체 |
| GLUT | `GLUT.framework` 사용 (`GLUT_Platform.h`) |
| 컨텍스트 | GLSL `#version 330`을 쓰려면 코어 프로파일이 필요 → macOS에서만 `GLUT_3_2_CORE_PROFILE` 추가 |
| VAO | 코어 프로파일은 VAO 바인딩이 필수 → `Renderer::Initialize()` 시작 시 VAO 생성/바인딩 |
| `stdafx.h` | `targetver.h`, `tchar.h`는 `#if defined(_WIN32)`로 감쌈 |
| 문자열 리터럴 | `ReadFile`/`CompileShaders` 매개변수를 `const char*`로 변경 (C++11 이후 필수) |
| 인코딩 | `Renderer.cpp`의 주석을 CP949 → UTF-8로 변환 |
