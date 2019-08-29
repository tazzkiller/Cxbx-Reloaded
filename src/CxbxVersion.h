#pragma once

#include "version.h"

#ifndef _GIT_VERSION
#define _GIT_VERSION "#unknown#"
#endif

/*! version string dependent on trace flag */
#ifndef _DEBUG_TRACE
#define _CXBX_VERSION _GIT_VERSION " (" __DATE__  ")"
#else
#define _CXBX_VERSION _GIT_VERSION "-Trace (" __DATE__  ")"
#endif
