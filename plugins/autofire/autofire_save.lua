local lib = {}

local function get_settings_path()
	return manager.machine.options.entries.homepath:value():match('([^;]+)') .. '/autofire'
end

local function get_settings_filename()
	return emu.romname() .. '.cfg'
end

local function initialize_button(settings)
	if settings.port and settings.mask and settings.type and settings.key and settings.on_frames and settings.off_frames then
		local ioport = manager.machine.ioport
		local new_button = {
			port = settings.port,
			mask = settings.mask,
			type = ioport:token_to_input_type(settings.type),
			key = manager.machine.input:seq_from_tokens(settings.key),
			key_cfg = settings.key,
			on_frames = settings.on_frames,
			off_frames = settings.off_frames,
			counter = 0
		}
		local port = ioport.ports[settings.port]
		if port then
			local field = port:field(settings.mask)
			if field and (field.type == new_button.type) then
				new_button.button = field
			end
		end
		return new_button
	end
	return nil
end

local function serialize_settings(button_list)
	local settings = {}
	for index, button in ipairs(button_list) do
		local setting = {
			port = button.port,
			mask = button.mask,
			type = manager.machine.ioport:input_type_to_token(button.type),
			key = button.key_cfg,
			on_frames = button.on_frames,
			off_frames = button.off_frames
		}
		table.insert(settings, setting)
	end
	return settings
end

-- Turbo out of the box: when a game has no autofire settings yet, give player 1's
-- Button 1 turbo on the controller's RB and Button 2 turbo on LB (by the controller's
-- own button names, so it works whatever number MAME gives them). Change or remove
-- them in the in-game menu under Plugin Options > Autofire.
local function default_buttons()
	local buttons = {}
	local input = manager.machine.input
	local joysticks = input.device_classes['joystick']
	if not joysticks then
		return buttons
	end
	local pad = joysticks.devices[1]
	if not pad then
		return buttons
	end

	local keys = {}
	for id, item in pairs(pad.items) do
		if item.name == 'RB' then
			keys['P1_BUTTON1'] = input:code_to_token(item.code)
		elseif item.name == 'LB' then
			keys['P1_BUTTON2'] = input:code_to_token(item.code)
		end
	end

	local ioport = manager.machine.ioport
	local used = {}
	for tag, port in pairs(ioport.ports) do
		for name, field in pairs(port.fields) do
			local token = ioport:input_type_to_token(field.type, field.player)
			local key = keys[token]
			if key and not used[token] then
				local new_button = initialize_button({
					port = tag,
					mask = field.mask,
					type = token,
					key = key,
					on_frames = 2,
					off_frames = 2 })
				if new_button and new_button.button then
					used[token] = true
					buttons[#buttons + 1] = new_button
				end
			end
		end
	end
	return buttons
end

function lib:load_settings()
	local buttons = {}
	local json = require('json')
	local filename = get_settings_path() .. '/' .. get_settings_filename()
	local file = io.open(filename, 'r')
	if not file then
		local ok, defaults = pcall(default_buttons)
		if ok and defaults then
			return defaults
		end
		return buttons
	end
	local loaded_settings = json.parse(file:read('a'))
	file:close()
	if not loaded_settings then
		emu.print_error(string.format('Error loading autofire settings: error parsing file "%s" as JSON', filename))
		return buttons
	end
	for index, button_settings in ipairs(loaded_settings) do
		local new_button = initialize_button(button_settings)
		if new_button then
			buttons[#buttons + 1] = new_button
		end
	end
	return buttons
end

function lib:save_settings(buttons)
	local path = get_settings_path()
	local attr = lfs.attributes(path)
	if attr and (attr.mode ~= 'directory') then
		emu.print_error(string.format('Error saving autofire settings: "%s" is not a directory', path))
		return
	end
	local filename = path .. '/' .. get_settings_filename()
	if #buttons == 0 then
		os.remove(filename)
		return
	elseif not attr then
		lfs.mkdir(path)
	end
	local json = require('json')
	local settings = serialize_settings(buttons)
	local data = json.stringify(settings, {indent = true})
	local file = io.open(filename, 'w')
	if not file then
		emu.print_error(string.format('Error saving autofire settings: error opening file "%s" for writing', filename))
		return
	end
	file:write(data)
	file:close()
end

return lib
