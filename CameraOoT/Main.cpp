#include <Windows.h>
#include <SDL.h>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <string>
#include <sstream>
#include <numbers>
#include "Proc.h"
#include "Time.h"

using namespace std;

unordered_map<string, string> ReadConfig(const string& filename) {
	unordered_map<string, string> config;
	ifstream file(filename);
	string line;
	while (getline(file, line)) {
		if (line.empty() || line[0] == '#') {
			continue;
		}
		istringstream iss(line);
		if (string key; getline(iss, key, '=')) {
			if (string value; getline(iss, value)) {
				config[key] = value;
			}
		}
	}
	return config;
}

unordered_map<string, SDL_GameControllerButton> button_map = {
  {"LEFT_SHOULDER_BUTTON", SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
  {"RIGHT_SHOULDER_BUTTON", SDL_CONTROLLER_BUTTON_RIGHTSHOULDER}
};

unordered_map<string, SDL_GameControllerAxis> axis_map = {
  {"LEFT_TRIGGER", SDL_CONTROLLER_AXIS_TRIGGERLEFT},
  {"RIGHT_TRIGGER", SDL_CONTROLLER_AXIS_TRIGGERRIGHT}
};

int main(int, char**)
{
	SetConsoleTitle(L"OoT Camera");

	const DWORD proc_id = GetProcId(L"citra-qt.exe");
	if (proc_id == 0)
	{
		cout << "citra-qt.exe not found" << endl;
		system("pause");
		return 0;
	}
	cout << "citra-qt.exe found" << endl;

	const HANDLE h_process = OpenProcess(PROCESS_ALL_ACCESS, NULL, proc_id);
	if (h_process == INVALID_HANDLE_VALUE)
	{
		cout << "Process invalid" << endl;
		system("pause");
		return 1;
	}
	cout << "Successfully hooked to citra-qt.exe, finding memory addresses..." << endl;

	const auto config = ReadConfig("CameraOoT.cfg");
	if (config.empty()) {
		cout << "Couldn't read \"CameraOoT.cfg\"" << endl;
		system("pause");
		return 1;
	}

	auto dead_zone_i = config.find("DEAD_ZONE_STICK");
	if (dead_zone_i == config.end()) {
		cout << "Error: Key 'DEAD_ZONE_STICK' not found in config" << endl;
		system("pause");
		return 1;
	}
	float dead_zone = 0.0f;
	try {
		dead_zone = stof(dead_zone_i->second);
	}
	catch (...) {
		cerr << "Error: Invalid DEAD_ZONE_STICK value '" << dead_zone_i->second << "'" << endl;
		system("pause");
		return 1;
	}

	auto x_speed_i = config.find("HORIZONTAL_SENSITIVITY");
	if (x_speed_i == config.end()) {
		cout << "Error: Key 'HORIZONTAL_SENSITIVITY' not found in config" << endl;
		system("pause");
		return 1;
	}
	float x_speed = 0.0f;
	try {
		x_speed = stof(x_speed_i->second);
	}
	catch (...) {
		cerr << "Error: Invalid HORIZONTAL_SENSITIVITY value '" << x_speed_i->second << "'" << endl;
		system("pause");
		return 1;
	}

	auto y_speed_i = config.find("VERTICAL_SENSITIVITY");
	if (y_speed_i == config.end()) {
		cout << "Error: Key 'VERTICAL_SENSITIVITY' not found in config" << endl;
		system("pause");
		return 1;
	}
	float y_speed = 0.0f;
	try {
		y_speed = stof(y_speed_i->second);
	}
	catch (...) {
		cerr << "Error: Invalid VERTICAL_SENSITIVITY value '" << y_speed_i->second << "'" << endl;
		return 1;
	}

	auto invert_x_i = config.find("INVERT_HORIZONTAL");
	if (invert_x_i == config.end()) {
		cout << "Error: Key 'INVERT_HORIZONTAL' not found in config" << endl;
		system("pause");
		return 1;
	}
	float invert_x = 1.0f;
	if (invert_x_i->second == "TRUE") {
		invert_x = -1.0f;
	}
	else if (invert_x_i->second != "FALSE") {
		cerr << "Error: Invalid INVERT_HORIZONTAL value '" << invert_x_i->second << "'" << endl;
		system("pause");
		return 1;
	}

	auto invert_y_i = config.find("INVERT_VERTICAL");
	if (invert_y_i == config.end()) {
		cout << "Error: Key 'INVERT_VERTICAL' not found in config" << endl;
		system("pause");
		return 1;
	}
	float invert_y = 1.0f;
	if (invert_y_i->second == "TRUE") {
		invert_y = -1.0f;
	}
	else if (invert_y_i->second != "FALSE") {
		cerr << "Error: Invalid INVERT_VERTICAL value '" << invert_y_i->second << "'" << endl;
		system("pause");
		return 1;
	}

	const auto reset_button_name_i = config.find("RESETCAMERA_BUTTON");
	if (reset_button_name_i == config.end()) {
		cout << "Error: Key 'RESETCAMERA_BUTTON' not found in config" << endl;
		system("pause");
		return 1;
	}
	const string reset_button_name = reset_button_name_i->second;
	bool reset_button_istrigger = false;
	if (reset_button_name == "LEFT_TRIGGER" || reset_button_name == "RIGHT_TRIGGER")
	{
		reset_button_istrigger = true;
	}

	SDL_GameControllerButton reset_button = {};
	SDL_GameControllerAxis reset_trigger = {};
	if (reset_button_istrigger)
	{
		if (!axis_map.contains(reset_button_name))
		{
			cout << "Error: Invalid RESETCAMERA_BUTTON value '" << reset_button_name << "'" << endl;
			system("pause");
			return 1;
		}
		reset_trigger = axis_map[reset_button_name];
	}
	else
	{
		if (!button_map.contains(reset_button_name))
		{
			cout << "Error: Invalid RESETCAMERA_BUTTON value '" << reset_button_name << "'" << endl;
			system("pause");
			return 1;
		}
		reset_button = button_map[reset_button_name];
	}

	const uint8_t pb_pattern_pp[17] = {
		0x2A, 0x00, 0x00, 0x60, 0x6A, 0x8F, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xFF, 0x75, 0x00 };
	const uint8_t pb_pattern_pc[17] = {
		0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x01 };
	const auto mask = "xxxxxxxxxxxxxxxx";
	const uintptr_t local_player = SearchInProcessMemory(h_process, pb_pattern_pp, mask) + 0x33;
	const uintptr_t link_crawl = local_player - 0x33 + 0x250B;
	const uintptr_t link_on_epona = local_player - 0x33 + 0x131;
	const uintptr_t link_climb = local_player - 0x33 + 0x2B1;
	const uintptr_t link_ocarina = local_player - 0x33 + 0xC1;
	const uintptr_t link_fixed = local_player - 0x33 + 0x89;
	printf("[+] Found LocalPlayer @ 0x%X\n", static_cast<unsigned>(local_player));
	const uintptr_t local_camera = SearchInProcessMemory(h_process, pb_pattern_pc, mask) + 0xB6;
	const uintptr_t look_at_camera = local_camera - 0xB6 + 0x60;
	printf("[+] Found LocalCamera @ 0x%X\n", static_cast<unsigned>(local_camera));

	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_GameController* controller = nullptr;
	SDL_Event event;

	Time time;
	float base_angle = 0.0f;
	float base_height = 50.0f;
	time.StartTime();
	float dx, dz, dy;
	constexpr uint16_t look_link = 60;
	uint16_t crawl_link = 11012;
	uint16_t epona_link = 0;
	uint16_t climb_link = 0;
	uint16_t ocarina_link = 2098;
	uint16_t fixed_link = 2178;

	float joystick_x = 0.0f;
	float joystick_y = 0.0f;
	bool reset_angle = true;
	bool pause = true;
	bool is_ocarina = false;

	while (true)
	{
		time.FixedUpdateTime();

		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_CONTROLLERDEVICEADDED:
				controller = SDL_GameControllerOpen(event.cdevice.which);
				if (controller)
				{
					cout << "[+] Controller " << event.cdevice.which << " is connected" << endl;
				}
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				SDL_GameControllerClose(controller);
				cout << "[X] Controller " << event.cdevice.which << " is not connected" << endl;
				break;
			case SDL_CONTROLLERBUTTONDOWN:
				if (!reset_button_istrigger && event.cbutton.button == reset_button)
				{
					reset_angle = true;
					pause = true;
				}
				break;
			case SDL_CONTROLLERAXISMOTION:
				if (event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX)
				{
					joystick_x = event.caxis.value / 32767.0f;
					if (joystick_x < dead_zone && joystick_x > -dead_zone)
					{
						joystick_x = 0.0f;
					}
					else
					{
						pause = false;
					}
				}
				else if (event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY)
				{
					joystick_y = event.caxis.value / 32767.0f;
					if (joystick_y < dead_zone && joystick_y > -dead_zone)
					{
						joystick_y = 0.0f;
					}
					else
					{
						pause = false;
					}
				}
				else if (reset_button_istrigger && event.caxis.axis == reset_trigger && event.caxis.value > 16384.0f)
				{
					reset_angle = true;
					pause = true;
				}
				break;
			default:
				break;
			}
		}

		ReadProcessMemory(h_process, reinterpret_cast<void*>(link_crawl), &crawl_link, sizeof(uint16_t), nullptr);
		ReadProcessMemory(h_process, reinterpret_cast<void*>(link_on_epona), &epona_link, sizeof(uint16_t), nullptr);
		ReadProcessMemory(h_process, reinterpret_cast<void*>(link_climb), &climb_link, sizeof(uint16_t), nullptr);
		ReadProcessMemory(h_process, reinterpret_cast<void*>(link_ocarina), &ocarina_link, sizeof(uint16_t), nullptr);
		ReadProcessMemory(h_process, reinterpret_cast<void*>(link_fixed), &fixed_link, sizeof(uint16_t), nullptr);

		if (ocarina_link == 2303 && !is_ocarina)
		{
			reset_angle = true;
			pause = true;
			is_ocarina = true;
		}
		else if (ocarina_link == 2098 && is_ocarina)
		{
			is_ocarina = false;
		}

		if (epona_link == 2448 || crawl_link == 21012 || climb_link == 143 || fixed_link == 2457)
		{
			reset_angle = true;
			pause = true;
		}

		if (!pause)
		{
			constexpr float length_base = 250.0f;
			WriteProcessMemory(h_process, reinterpret_cast<void*>(look_at_camera), &look_link, sizeof(uint16_t), nullptr);
			ReadProcessMemory(h_process, reinterpret_cast<void*>(local_player), &x, sizeof(float), nullptr);
			ReadProcessMemory(h_process, reinterpret_cast<void*>(local_player + 0x04), &y, sizeof(float), nullptr);
			ReadProcessMemory(h_process, reinterpret_cast<void*>(local_player + 0x08), &z, sizeof(float), nullptr);

			if (reset_angle)
			{
				reset_angle = false;
				ReadProcessMemory(h_process, reinterpret_cast<void*>(local_camera), &dx, sizeof(float), nullptr);
				ReadProcessMemory(h_process, reinterpret_cast<void*>(local_camera + 0x04), &dy, sizeof(float), nullptr);
				ReadProcessMemory(h_process, reinterpret_cast<void*>(local_camera + 0x08), &dz, sizeof(float), nullptr);
				base_angle = atan2(x - dx, z - dz) * -180.0f / numbers::pi_v<float>;
				base_angle -= 90.0f;
				if (base_angle < -180.0f)
				{
					base_angle += 360.0f;
				}
				else if (base_angle > 180.0f)
				{
					base_angle -= 360.0f;
				}
			}

			base_angle += time.GetFixedDeltaTime() * joystick_x * x_speed * invert_x;
			if (base_angle > 180.0f)
			{
				base_angle -= 360.0f;
			}
			else if (base_angle < -180.0f)
			{
				base_angle += 360.0f;
			}

			base_height += time.GetFixedDeltaTime() * joystick_y * y_speed * invert_y;
			if (base_height > 360.0f)
			{
				base_height = 360.0f;
			}
			else if (base_height < -180.0f)
			{
				base_height = -180.0f;
			}

			const float theta = base_angle * numbers::pi_v<float> / 180.0f;
			dx = (cos(theta) * length_base) + x;
			dy = base_height + y;
			dz = (sin(theta) * length_base) + z;

			if (!reset_angle)
			{
				WriteProcessMemory(h_process, reinterpret_cast<void*>(local_camera), &dx, sizeof(float), nullptr);
				WriteProcessMemory(h_process, reinterpret_cast<void*>(local_camera + 0x04), &dy, sizeof(float), nullptr);
				WriteProcessMemory(h_process, reinterpret_cast<void*>(local_camera + 0x08), &dz, sizeof(float), nullptr);
			}
		}
	}
}
