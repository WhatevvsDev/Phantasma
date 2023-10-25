#pragma once

// windows.h: disable as much as possible to speed up compilation.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#endif
#define NOGDICAPMASKS
// #define NOVIRTUALKEYCODES <- Required for GLFW (?) I think
//#define NOWINMESSAGES <- Required for ImGUI
//#define NOWINSTYLES <- Required for ImGUI
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOKEYSTATES
#define NOSYSCOMMANDS
#define NORASTEROPS
#define NOSHOWWINDOW
#define OEMRESOURCE
#define NOATOM
//#define NOCLIPBOARD <- Required for ImGUI
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOKERNEL
//#define NONLS <- Required for unicode support(?) in TinyGLTF I think
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOMSG
#define NOOPENFILE
#define NOSCROLL
#define NOTEXTMETRIC
#define NOWH
//#define NOWINOFFSETS <- Required for ImGUi 
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#include "windows.h"
