#pragma once

//
// Cross-platform GLUT header.
//
//   Windows : uses the bundled freeglut (Dependencies/freeglut.h).
//   macOS   : uses the system GLUT framework.
//

#if defined(__APPLE__)

#define GL_SILENCE_DEPRECATION
#include <GLUT/glut.h>

#else

#include "freeglut.h"

#endif
