#include <filesystem>
#include <utility>
#include <not_implemented.h>
#include "../include/client_logger_builder.h"
#include <not_implemented.h>

using namespace nlohmann;

logger_builder& client_logger_builder::add_file_stream(
    std::string const &stream_file_path,
    logger::severity severity) &
{

    auto openstream = _output_streams.find(severity);
    if (openstream == _output_streams.end()) {
        openstream = _output_streams.emplace(severity, std::make_pair(std::forward_list<client_logger::refcounted_stream>(), false)).first;
    }
    openstream->second.first.emplace_front(std::filesystem::weakly_canonical(stream_file_path).string());// делаем относительны й путь
    return *this;
}

logger_builder& client_logger_builder::add_console_stream(
    logger::severity severity) &
{

    auto openstream = _output_streams.find(severity);
    if (openstream == _output_streams.end()) {
        _output_streams.emplace(severity, std::make_pair(std::forward_list<client_logger::refcounted_stream>(), true)).first;
    }
    else
    {
        openstream->second.second = true;
    }

    return *this;
}

logger_builder& client_logger_builder::transform_with_configuration(
    std::string const &configuration_file_path,
    std::string const &configuration_path) &
{
    std::ifstream file;
    file.open(configuration_file_path);
    if (!file.is_open()) throw std::ios_base::failure("cant open file " + configuration_file_path);

    json data = json::parse(file);
    file.close();

    auto openstream = data.find(configuration_path);
    if (openstream == data.end() || !openstream->is_object()) return *this;//нет своваря в словаре

    parse_severity(logger::severity::information, (*openstream)["information"]);
    parse_severity(logger::severity::critical, (*openstream)["critical"]);
    parse_severity(logger::severity::warning, (*openstream)["warning"]);
    parse_severity(logger::severity::trace, (*openstream)["trace"]);
    parse_severity(logger::severity::debug, (*openstream)["debug"]);
    parse_severity(logger::severity::error, (*openstream)["error"]);

    auto format = openstream->find("format");
    if (format != openstream->end() && format->is_string()) {
        _format = format.value();
    }
    return *this;
}

logger_builder& client_logger_builder::clear() &
{
    _output_streams.clear();
    _format = "%m";
    return *this;
}

logger *client_logger_builder::build() const
{
    return new client_logger(_output_streams, _format);
}

logger_builder& client_logger_builder::set_format(const std::string &format) &
{
    _format = format;
    return *this;
}

void client_logger_builder::parse_severity(logger::severity sev, nlohmann::json& j)
{

if (j.empty() || !j.is_object()) return;

	auto opened_stream = _output_streams.find(sev);

	auto data_paths = j.find("paths");
	if (data_paths != j.end() && data_paths->is_array()) {
		json data = *data_paths;
		for (const json& js: data) {
			if (js.empty() || !js.is_string()) continue;

			const std::string& path = js;
			if (opened_stream == _output_streams.end()) {
				opened_stream = _output_streams.emplace(sev, std::make_pair(std::forward_list<client_logger::refcounted_stream>(), false)).first;
			}
			opened_stream->second.first.emplace_front(std::filesystem::weakly_canonical(path).string());
		}
	}

	auto console = j.find("console");
	if (console != j.end() && console->is_boolean()) {
		if (opened_stream == _output_streams.end()) {
			opened_stream = _output_streams.emplace(sev, std::make_pair(std::forward_list<client_logger::refcounted_stream>(), false)).first;
		}
		opened_stream->second.second = console->get<bool>();
	}
 }

logger_builder& client_logger_builder::set_destination(const std::string &format) &
{
    return  *this;
}
