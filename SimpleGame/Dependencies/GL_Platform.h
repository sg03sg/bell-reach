#pragma once

//
// Cross-platform OpenGL loader header.
//
//   Windows : uses the bundled GLEW (Dependencies/glew.h) exactly as before.
//   macOS   : uses the system OpenGL framework (Core Profile 3.3 / 4.1).
//             macOS exports every core entry point directly, so no loader
//             library is needed. glewInit()/glewIsSupported() are provided
//             below as no-op shims so the existing code compiles unchanged.
//

#if defined(__APPLE__)

#define GL_SILENCE_DEPRECATION
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#include <cstring>

inline int glewInit()
{
	return 0; // GLEW_OK
}

inline bool glewIsSupported(const char* /*name*/)
{
	// The GLUT core profile context created in main() is 3.3+ on every
	// Mac that can run this project.
	return true;
}

#else

#include "glew.h"

#endif
