//
// Created by Des Caldnd on 3/27/2024.
//

#include "server.h"

#include <logger_builder.h>

#include <fstream>
#include <iostream>


server::server(uint16_t port) {
	CROW_ROUTE(app, "/init")([&](const crow::request &req) {
		std::string pid_string = req.url_params.get("pid");
		std::string sev_string = req.url_params.get("sev");
		std::string path_string = req.url_params.get("path");
		std::string console_string = req.url_params.get("console");

		std::cout << "ИНИЦИАЛИЗИРОВАННЫЙ PID: " << pid_string << " SEVERITY: " << sev_string << " PATH: " << path_string << " CONSOLE: " << console_string << std::endl;

		int pid = std::stoi(pid_string);
		logger::severity sev = logger_builder::string_to_severity(sev_string);
		bool console = console_string == "1";

		std::lock_guard lock(_mut);
		auto it = _streams.find(pid);

		if (it == _streams.end()) {
			it = _streams.emplace(pid, std:: unordered_map<logger::severity, std::pair<std::string, bool>>()).first;
		}

		auto inner_it = it->second.find(sev);

		if (inner_it == it->second.end()) {
			inner_it = it->second.emplace(sev, std::make_pair(std::string(), false)).first;
		}

		inner_it->second.first = std::move(path_string);
		inner_it->second.second = console;

		return crow::response(204);
	});

	CROW_ROUTE(app, "/delete")([&](const crow::request &req) {
		std::string pid_string = req.url_params.get("pid");

		std::cout << "УДАЛЯЕМ PID: " << pid_string << std::endl;

		int pid = std::stoi(pid_string);

		std::lock_guard lock(_mut);
		_streams.erase(pid);

		return crow::response(204);
	});

	CROW_ROUTE(app, "/log")([&](const crow::request &req) {
		std::string pid_str = req.url_params.get("pid");
		std::string sev_str = req.url_params.get("sev");
		std::string message = req.url_params.get("message");

		std::cout << "ЛОГ ДЛЯ PID: " << pid_str << " SEVERITY: " << sev_str << " MESSAGE: " << message << std::endl;

		int pid = std::stoi(pid_str);
		logger::severity sev = logger_builder::string_to_severity(sev_str);

		std::shared_lock lock(_mut);
		auto it = _streams.find(pid);
		if (it != _streams.end()) {
			auto inner_it = it->second.find(sev);

			if (inner_it != it->second.end()) {
				const std::string &path = inner_it->second.first;
				if (!path.empty()) {
					std::ofstream stream(path, std::ios_base::app);
					if (stream.is_open()) {
						stream << message << std::endl;
					}
				}

				if (inner_it->second.second) {
					std::cout << message << std::endl;
				}
			}
		}
		return crow::response(204);
	});


	app.port(port).loglevel(crow::LogLevel::Warning).multithreaded();
	app.run();
}