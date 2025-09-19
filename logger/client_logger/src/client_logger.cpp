#include <string>
#include <sstream>
#include <algorithm>
#include <utility>
#include "../include/client_logger.h"
#include <not_implemented.h>

std::unordered_map<std::string, std::pair<size_t, std::ofstream>> client_logger::refcounted_stream::_global_streams;


logger& client_logger::log(
    const std::string &text,
    logger::severity severity) &
{
    std::string out = make_format(text, severity);
    auto openstream = _output_streams.find(severity);
    if (openstream == _output_streams.end()) {
        return *this;
    }

    if (openstream->second.second) {
        std::cout << out << std::endl;
    }

    for (auto &stream: openstream->second.first) {
        std::ofstream *ofstream = stream._stream.second;
        if (ofstream != nullptr) {
            *ofstream << out << std::endl;
        }
    }
    return *this;


}

std::string client_logger::make_format(const std::string &message, severity sev) const
{
    std::ostringstream os;
    bool is_escape = false;

    for (char c : _format) {
        if (is_escape) {

            switch (char_to_flag(c)) {
            case flag::DATE:
                os << current_date_to_string();
                break;
            case flag::TIME:
                os << current_time_to_string();
                break;
            case flag::SEVERITY:
                os << severity_to_string(sev);
                break;
            case flag::MESSAGE:
                os << message;
                break;
            default:

                    os << '%' << c;
                break;
            }
            is_escape = false;
        } else if (c == '%') {

            is_escape = true;
        } else {

            os << c;
        }
    }


    if (is_escape) {
        os << '%';
    }

    return os.str();
}

client_logger::client_logger(
        const std::unordered_map<logger::severity, std::pair<std::forward_list<refcounted_stream>, bool>> &streams,
        std::string format): _output_streams(streams), _format(std::move(format))
{
}

client_logger::flag client_logger::char_to_flag(char c) noexcept
{
    switch (c)
    {
    case 'd':
        return flag::DATE;
    case 't':
        return flag::TIME;
    case 'm':
        return flag::MESSAGE;
    case 's':
        return flag::SEVERITY;
    default:
        return flag::NO_FLAG;

    }

}

client_logger::client_logger(const client_logger &other):_output_streams(other._output_streams), _format(other._format)
{}

client_logger &client_logger::operator=(const client_logger &other)
{
    if (this == &other) return *this;
    else
    {
        _output_streams = other._output_streams;
        _format = other._format;
        return *this;
    }
}

client_logger::client_logger(client_logger &&other) noexcept: _output_streams(std::move(other._output_streams)), _format(std::move(other._format))
{
}

client_logger &client_logger::operator=(client_logger &&other) noexcept
{
    if (this == &other) return *this;
    _output_streams = std::move(other._output_streams);
    _format = std::move(other._format);
    return *this;
}


client_logger::~client_logger() noexcept = default;


client_logger::refcounted_stream::refcounted_stream(const std::string &path)//поправить
{
    auto stream_open = _global_streams.find(path);

    if (stream_open == _global_streams.end()) {
        auto inserted_stream = _global_streams.emplace(path, std::make_pair(1, std::ofstream(path)));//итератор и буль

        auto &stream = inserted_stream.first->second;//size_t и поток

        if (!stream.second.is_open()) {
            throw std::ios_base::failure("Can't open file " + path);
        }

        _stream = std::make_pair(path, &inserted_stream.first->second.second);
    } else {
        stream_open->second.first++;
        _stream = std::make_pair(path, &stream_open->second.second);
    }

}

client_logger::refcounted_stream::refcounted_stream(const client_logger::refcounted_stream &oth)
{
    auto openstream = _global_streams.find(oth._stream.first);//поиск

    if (openstream != _global_streams.end()) {
        ++openstream->second.first;
        _stream.second = &openstream->second.second;
    } else {
        auto inserted = _global_streams.emplace(_stream.first, std::make_pair<size_t>(1, std::ofstream(oth._stream.first)));
        if (inserted.second || !inserted.first->second.second.is_open())
        {
            _global_streams.erase(inserted.first);
            throw std::ios_base::failure("Can't open file " + oth._stream.first);
            //записалось норм но не откралось
        }
        else if (!inserted.second || !inserted.first->second.second.is_open())
        { //либо эл-т уже есть либо не получилосб открыть файл

            throw std::ios_base::failure("Can't open file " + oth._stream.first);
        }
        _stream.second = &inserted.first->second.second;//приравниваем адрес
    }
}

client_logger::refcounted_stream &
client_logger::refcounted_stream::operator=(const client_logger::refcounted_stream &oth)
{
    if (this == &oth) return *this;
    if (_stream.second != nullptr) {
        auto openstream = _global_streams.find(_stream.first);

        if (openstream != _global_streams.end()) {
            --openstream->second.first;

            if (openstream->second.first == 0) {
                openstream->second.second.close();
                _global_streams.erase(openstream);
            }
        }
    }

    _stream.first = oth._stream.first;
    _stream.second = oth._stream.second;

    if (_stream.second != nullptr) {
        auto opened_stream = _global_streams.find(_stream.first);
        ++opened_stream->second.first;
    }
    return *this;
}

client_logger::refcounted_stream::refcounted_stream(client_logger::refcounted_stream &&oth) noexcept : _stream(std::move(oth._stream))
{
    oth._stream.second = nullptr;
}

client_logger::refcounted_stream &client_logger::refcounted_stream::operator=(client_logger::refcounted_stream &&oth) noexcept
{
    if (this != &oth)
    {
        _stream = std::move(oth._stream);
        oth._stream.second = nullptr;
    }
    return *this;
}

client_logger::refcounted_stream::~refcounted_stream()
{
    if (_stream.second!=nullptr)
    {
        auto openstream = _global_streams.find(_stream.first);
        if (openstream != _global_streams.end())
        {
            --openstream->second.first;
            if (openstream->second.first == 0) {
                openstream->second.second.close();
                _global_streams.erase(openstream);
            }
        }
    }

}
