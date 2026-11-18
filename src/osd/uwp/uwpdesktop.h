// license:BSD-3-Clause
// copyright-holders:MAMEdev Team
//============================================================
//
//  uwpdesktop.h - forced include for the Xbox (UWP) build
//
//  Visual Studio builds a Universal Windows app with only the
//  "app" subset of the Windows API visible. Xbox Dev Mode runs
//  ordinary Win32 code fine (other Xbox emulator ports rely on
//  this), so we switch every project to the desktop API set and
//  only use the UWP-specific APIs (C++/WinRT) in src/osd/uwp.
//
//  bx, bimg and bgfx define UWP_KEEP_APP_PARTITION so bgfx
//  keeps its WinRT path and creates its swap chain for the
//  CoreWindow.
//
//============================================================

#ifndef MAME_OSD_UWP_UWPDESKTOP_H
#define MAME_OSD_UWP_UWPDESKTOP_H

#if !defined(UWP_KEEP_APP_PARTITION)
#undef WINAPI_FAMILY
#define WINAPI_FAMILY 100 /* WINAPI_FAMILY_DESKTOP_APP */
#endif

// Universal Windows projects turn on UNICODE everywhere; the desktop
// build only uses it for the OSD layer, which asks for it explicitly.
#if !defined(UWP_WANT_UNICODE)
#undef UNICODE
#undef _UNICODE
#endif

#endif // MAME_OSD_UWP_UWPDESKTOP_H
