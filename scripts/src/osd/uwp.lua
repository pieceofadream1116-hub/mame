-- license:BSD-3-Clause
-- copyright-holders:MAMEdev Team

---------------------------------------------------------------------------
--
--   uwp.lua
--
--   Rules for the building of UWP OSD (Xbox / Universal Windows app)
--
---------------------------------------------------------------------------

dofile("modules.lua")

function maintargetosdoptions(_target,_subtarget)
	osdmodulestargetconf()

	links {
		"bcrypt",
		"dxgi",
		"d3d11",
		"windowsapp",
	}
end

project ("osd_" .. _OPTIONS["osd"])
	uuid (os.uuid("osd_" .. _OPTIONS["osd"]))
	kind (LIBTYPE)

	removeflags {
		"SingleOutputDir",
	}

	dofile("uwp_cfg.lua")
	osdmodulesbuild()

	includedirs {
		MAME_DIR .. "src/emu",
		MAME_DIR .. "src/devices", -- accessing imagedev from debugger
		MAME_DIR .. "src/osd",
		MAME_DIR .. "src/lib",
		MAME_DIR .. "src/lib/util",
		MAME_DIR .. "src/osd/modules/file",
		MAME_DIR .. "src/osd/modules/render",
		MAME_DIR .. "3rdparty",
	}

	includedirs {
		MAME_DIR .. "src/osd/uwp",
		MAME_DIR .. "src/osd/windows",
	}

	files {
		MAME_DIR .. "src/osd/uwp/video.cpp",
		MAME_DIR .. "src/osd/uwp/video.h",
		MAME_DIR .. "src/osd/uwp/window.cpp",
		MAME_DIR .. "src/osd/uwp/window.h",
		MAME_DIR .. "src/osd/modules/osdwindow.cpp",
		MAME_DIR .. "src/osd/modules/osdwindow.h",
		MAME_DIR .. "src/osd/windows/winmain.cpp",
		MAME_DIR .. "src/osd/windows/winmain.h",
		MAME_DIR .. "src/osd/windows/winopts.cpp",
		MAME_DIR .. "src/osd/windows/winopts.h",
		MAME_DIR .. "src/osd/uwp/uwpcompat.cpp",
		MAME_DIR .. "src/osd/uwp/uwpcompat.h",
		MAME_DIR .. "src/osd/osdepend.h",
	}

project ("ocore_" .. _OPTIONS["osd"])
	uuid (os.uuid("ocore_" .. _OPTIONS["osd"]))
	kind (LIBTYPE)

	removeflags {
		"SingleOutputDir",
	}

	dofile("uwp_cfg.lua")

	includedirs {
		MAME_DIR .. "3rdparty",
		MAME_DIR .. "src/emu",
		MAME_DIR .. "src/osd",
		MAME_DIR .. "src/osd/modules/file",
		MAME_DIR .. "src/lib",
		MAME_DIR .. "src/lib/util",
		ext_includedir("asio"),
	}

	BASE_TARGETOS = "win32"
	SDLOS_TARGETOS = "win32"

	includedirs {
		MAME_DIR .. "src/osd/uwp",
		MAME_DIR .. "src/osd/windows",
	}

	files {
		MAME_DIR .. "src/osd/asio.cpp",
		MAME_DIR .. "src/osd/asio.h",
		MAME_DIR .. "src/osd/eigccppc.h",
		MAME_DIR .. "src/osd/eigccx86.h",
		MAME_DIR .. "src/osd/eivc.h",
		MAME_DIR .. "src/osd/eivcarm.h",
		MAME_DIR .. "src/osd/eivcx86.h",
		MAME_DIR .. "src/osd/eminline.h",
		MAME_DIR .. "src/osd/osdcomm.h",
		MAME_DIR .. "src/osd/osdcore.cpp",
		MAME_DIR .. "src/osd/osdcore.h",
		MAME_DIR .. "src/osd/strconv.cpp",
		MAME_DIR .. "src/osd/strconv.h",
		MAME_DIR .. "src/osd/osdsync.cpp",
		MAME_DIR .. "src/osd/osdsync.h",
		MAME_DIR .. "src/osd/windows/winutf8.cpp",
		MAME_DIR .. "src/osd/windows/winutf8.h",
		MAME_DIR .. "src/osd/windows/winutil.cpp",
		MAME_DIR .. "src/osd/windows/winutil.h",
		MAME_DIR .. "src/osd/modules/osdmodule.cpp",
		MAME_DIR .. "src/osd/modules/osdmodule.h",
		MAME_DIR .. "src/osd/modules/file/winrtdir.cpp",
		MAME_DIR .. "src/osd/modules/file/winrtfile.cpp",
		MAME_DIR .. "src/osd/modules/file/winrtfile.h",
		MAME_DIR .. "src/osd/modules/file/winrtptty.cpp",
		MAME_DIR .. "src/osd/modules/file/winrtsocket.cpp",
		MAME_DIR .. "src/osd/modules/lib/osdlib_uwp.cpp",
	}
