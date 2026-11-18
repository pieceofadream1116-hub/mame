// license:BSD-3-Clause
// copyright-holders:MAMEdev Team
//============================================================
//
//  uwpinput.cpp - Xbox (UWP) controllers
//
//  Xbox controllers are read with Windows.Gaming.Input and
//  handed to MAME's XInput module in XInput's own format, so
//  the controller gets that module's full default mappings
//  (UI navigation, players 1-4, analog sticks, triggers).
//
//============================================================

#include <windows.h>
#include <xinput.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

#undef interface

using namespace winrt::Windows::Gaming::Input;


namespace {

std::mutex g_pad_lock;
std::vector<Gamepad> g_pads; // in the order they were connected
bool g_started = false;

void add_pad(Gamepad const &pad)
{
	std::lock_guard<std::mutex> lock(g_pad_lock);
	if (std::find(g_pads.begin(), g_pads.end(), pad) == g_pads.end())
		g_pads.push_back(pad);
}

void remove_pad(Gamepad const &pad)
{
	std::lock_guard<std::mutex> lock(g_pad_lock);
	auto const found = std::find(g_pads.begin(), g_pads.end(), pad);
	if (found != g_pads.end())
		g_pads.erase(found);
}

Gamepad pad_at(DWORD index)
{
	std::lock_guard<std::mutex> lock(g_pad_lock);
	if (index < g_pads.size())
		return g_pads[index];
	return nullptr;
}

SHORT stick(double value)
{
	double const scaled = std::clamp(value, -1.0, 1.0) * 32767.0;
	return SHORT(std::lround(scaled));
}

BYTE trigger(double value)
{
	return BYTE(std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
}

} // anonymous namespace


//============================================================
//  uwp_input_start - called once at app start, so the
//  controllers are known before MAME looks for them
//============================================================

void uwp_input_start()
{
	if (g_started)
		return;
	g_started = true;

	Gamepad::GamepadAdded([] (auto const &, Gamepad const &pad) { add_pad(pad); });
	Gamepad::GamepadRemoved([] (auto const &, Gamepad const &pad) { remove_pad(pad); });

	for (Gamepad const &pad : Gamepad::Gamepads())
		add_pad(pad);
}


//============================================================
//  uwp_input_count - number of controllers connected now
//============================================================

unsigned uwp_input_count()
{
	std::lock_guard<std::mutex> lock(g_pad_lock);
	return unsigned(g_pads.size());
}


//============================================================
//  XInput-shaped entry points used by input_xinput.cpp
//============================================================

DWORD WINAPI uwp_xinput_get_state(DWORD dwUserIndex, XINPUT_STATE *pState)
{
	Gamepad const pad = pad_at(dwUserIndex);
	if (!pad)
		return ERROR_DEVICE_NOT_CONNECTED;

	GamepadReading const reading = pad.GetCurrentReading();
	auto const has = [&reading] (GamepadButtons button) { return (reading.Buttons & button) == button; };

	WORD buttons = 0;
	if (has(GamepadButtons::DPadUp))          buttons |= XINPUT_GAMEPAD_DPAD_UP;
	if (has(GamepadButtons::DPadDown))        buttons |= XINPUT_GAMEPAD_DPAD_DOWN;
	if (has(GamepadButtons::DPadLeft))        buttons |= XINPUT_GAMEPAD_DPAD_LEFT;
	if (has(GamepadButtons::DPadRight))       buttons |= XINPUT_GAMEPAD_DPAD_RIGHT;
	if (has(GamepadButtons::Menu))            buttons |= XINPUT_GAMEPAD_START;
	if (has(GamepadButtons::View))            buttons |= XINPUT_GAMEPAD_BACK;
	if (has(GamepadButtons::LeftThumbstick))  buttons |= XINPUT_GAMEPAD_LEFT_THUMB;
	if (has(GamepadButtons::RightThumbstick)) buttons |= XINPUT_GAMEPAD_RIGHT_THUMB;
	if (has(GamepadButtons::LeftShoulder))    buttons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
	if (has(GamepadButtons::RightShoulder))   buttons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
	if (has(GamepadButtons::A))               buttons |= XINPUT_GAMEPAD_A;
	if (has(GamepadButtons::B))               buttons |= XINPUT_GAMEPAD_B;
	if (has(GamepadButtons::X))               buttons |= XINPUT_GAMEPAD_X;
	if (has(GamepadButtons::Y))               buttons |= XINPUT_GAMEPAD_Y;

	pState->dwPacketNumber = DWORD(reading.Timestamp);
	pState->Gamepad.wButtons = buttons;
	pState->Gamepad.bLeftTrigger = trigger(reading.LeftTrigger);
	pState->Gamepad.bRightTrigger = trigger(reading.RightTrigger);
	pState->Gamepad.sThumbLX = stick(reading.LeftThumbstickX);
	pState->Gamepad.sThumbLY = stick(reading.LeftThumbstickY);
	pState->Gamepad.sThumbRX = stick(reading.RightThumbstickX);
	pState->Gamepad.sThumbRY = stick(reading.RightThumbstickY);
	return ERROR_SUCCESS;
}


DWORD WINAPI uwp_xinput_get_capabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES *pCapabilities)
{
	if (!pad_at(dwUserIndex))
		return ERROR_DEVICE_NOT_CONNECTED;

	// a standard Xbox controller: every button, both sticks and both triggers
	*pCapabilities = XINPUT_CAPABILITIES{};
	pCapabilities->Type = XINPUT_DEVTYPE_GAMEPAD;
	pCapabilities->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
	pCapabilities->Flags = 0;
	pCapabilities->Gamepad.wButtons =
			XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT |
			XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_LEFT_THUMB | XINPUT_GAMEPAD_RIGHT_THUMB |
			XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER |
			XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y;
	pCapabilities->Gamepad.bLeftTrigger = 0xff;
	pCapabilities->Gamepad.bRightTrigger = 0xff;
	pCapabilities->Gamepad.sThumbLX = SHORT(0x7fff);
	pCapabilities->Gamepad.sThumbLY = SHORT(0x7fff);
	pCapabilities->Gamepad.sThumbRX = SHORT(0x7fff);
	pCapabilities->Gamepad.sThumbRY = SHORT(0x7fff);
	pCapabilities->Vibration.wLeftMotorSpeed = 0xffff;
	pCapabilities->Vibration.wRightMotorSpeed = 0xffff;
	return ERROR_SUCCESS;
}
