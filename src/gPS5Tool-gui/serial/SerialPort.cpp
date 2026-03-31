// Copyright (c) 2026, legroeder <me@legroeder.rocks>
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//     list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "SerialPort.h"
#include <cstring>
#include <fcntl.h>
#include <format>
#include <poll.h>
#include <stdexcept>
#include <termios.h>
#include <unistd.h>
#include <systemd/sd-device.h>

using namespace std;

SerialPort::SerialPort() = default;
SerialPort::SerialPort(const SerialPortEntry& serialPort) : _deviceName(serialPort.deviceNode)
{ }

SerialPort::~SerialPort()
{
    if (_fd >= 0)
    {
        disconnect();
    }
}

void SerialPort::connect()
{
    _fd = open(_deviceName.c_str(), O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
    if (_fd < 0) throw std::runtime_error(std::format("Could not open serial port {}: {}", _deviceName, std::string(strerror(errno))));
}

void SerialPort::disconnect()
{
    if (_fd >= 0)
    {
        close(_fd);
        _fd = -1;
    }
}

void SerialPort::configurePort() const
{
    if (_fd < 0) throw std::runtime_error("Serial port not opened");
    termios tty{};
    if (tcgetattr(_fd, &tty) < 0) throw std::runtime_error("Could not configure serial port: " + std::string(strerror(errno)));

    // Output baud rate
    if (cfsetospeed(&tty, B115200) < 0) throw std::runtime_error("Could not set baud rate: " + std::string(strerror(errno)));
    if (cfsetispeed(&tty, B115200) < 0) throw std::runtime_error("Could not set baud rate: " + std::string(strerror(errno)));

    // Control modes: 8N1, no flow control, enable receiver, ignore modem ctrl lines
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    // Local modes: raw input
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);

    // Disable special handling
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(_fd, TCSANOW, &tty) < 0) throw std::runtime_error("Could not configure serial port: " + std::string(strerror(errno)));
}

void SerialPort::setEcmMode(const bool ecmMode)
{
    _ecmMode = ecmMode;
}

void SerialPort::send(const std::string& data) const
{
    if (_fd < 0) throw std::runtime_error("Serial port not opened");

    string toSend = data;

    if (_ecmMode)
    {
        ssize_t sum = 0;
        for (const char c : toSend)
        {
            sum += c;
        }
        toSend += format(":{:X}", sum & 0xff);
    }

    // Append a newline
    toSend += "\n";

    // And send it out over serial :)
    size_t total = 0;
    while (total < toSend.size())
    {
        const size_t n = write(_fd, toSend.data() + total, toSend.size() - total);
        if (n == 0) throw std::runtime_error("Could not write to serial port: " + std::string(strerror(errno)));
        total += n;
    }

    tcdrain(_fd);
}

bool SerialPort::waitForData(const int timeoutMs) const
{
    if (_fd < 0) throw std::runtime_error("Serial port not opened");

    pollfd pfd{};
    pfd.fd = _fd;
    pfd.events = POLLIN;

    while (true)
    {
        const int rc = poll(&pfd, 1, timeoutMs);
        if (rc > 0)
        {
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
                throw std::runtime_error("Serial port poll failed or port disconnected!");

            return (pfd.events & POLLIN) != 0;
        }

        if (rc == 0)
            return false; // Timeout

        if (errno == EINTR)
            continue;

        throw std::runtime_error(std::format("Could not poll serial port: {}", strerror(errno)));
    }
}

void SerialPort::readAvailable()
{
    if (_fd < 0) throw std::runtime_error("Serial port not opened");

    char buffer[256];

    while (true)
    {
        const ssize_t n = read(_fd, buffer, sizeof(buffer));
        if (n > 0)
        {
            _readBuffer.append(buffer, static_cast<size_t>(n));

            // If we received less than the buffer size, chances are high that we've drained what is currently queued
            if (n < static_cast<ssize_t>(sizeof(buffer)))
                return;

            continue;
        }

        if (n == 0)
        {
            return; // Uncommon for tty devices, but possible if the other end closes the connection
        }

        if (errno == EINTR)
            continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;

        throw std::runtime_error(std::format("Could not read from serial port: {}", strerror(errno)));
    }
}

bool SerialPort::tryExtractLine(std::string& outLine)
{
    const size_t pos = _readBuffer.find_first_of("\r\n");
    if (pos == std::string::npos)
        return false;

    outLine = _readBuffer.substr(0, pos);

    size_t eraseLen = pos;
    while (eraseLen < _readBuffer.size() &&
           (_readBuffer[eraseLen] == '\r' || _readBuffer[eraseLen] == '\n'))
    {
        ++eraseLen;
    }

    _readBuffer.erase(0, eraseLen);
    return true;
}

optional<string> SerialPort::readLine(const int timeoutMs)
{
    if (_fd < 0) throw std::runtime_error("Serial port not opened");

    std::string line;

    // Maybe we already have a complete line buffered
    if (tryExtractLine(line))
        return line;

    int elapsedMs = 0;

    while (elapsedMs < timeoutMs)
    {
        constexpr int pollSliceMs = 50;
        const int waitMs = std::min(pollSliceMs, timeoutMs - elapsedMs);

        if (!waitForData(waitMs))
        {
            elapsedMs += waitMs;
            continue;
        }

        readAvailable();

        if (tryExtractLine(line))
            return line;

        elapsedMs += waitMs;
    }

    return std::nullopt;
}

vector<SerialPortEntry> SerialPort::getSerialDevices()
{
    static const vector<string> prefixes = {
        "/dev/ttyS",
        "/dev/ttyUSB",
        "/dev/ttyACM",
        "/dev/ttyAMA",
        "/dev/ttyTHS"
    };

    vector<SerialPortEntry> ports;

    sd_device_enumerator* enumerator = nullptr;
    if (sd_device_enumerator_new(&enumerator) < 0 || !enumerator)
        return {};

    // Only look at tty subsystem devices
    if (sd_device_enumerator_add_match_subsystem(enumerator, "tty", true) < 0)
    {
        sd_device_enumerator_unref(enumerator);
        return {};
    }

    for (sd_device* dev = sd_device_enumerator_get_device_first(enumerator);
         dev != nullptr;
         dev = sd_device_enumerator_get_device_next(enumerator))
    {
        const char* devnode = nullptr;
        if (sd_device_get_devname(dev, &devnode) < 0 || !devnode)
            continue;

        std::string devnodeStr = devnode;
        std::string description;

        // Match only the device nodes we actually want
        bool matchesPrefix = false;
        for (const auto& prefix : prefixes)
        {
            if (devnodeStr.starts_with(prefix))
            {
                matchesPrefix = true;
                break;
            }
        }

        if (!matchesPrefix)
            continue;

        // Check for USB parent (if any)
        sd_device* parent = nullptr;
        if (sd_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device", &parent) >= 0 && parent)
        {
            const char* vendor = nullptr;
            const char* product = nullptr;
            const char* serial = nullptr;

            sd_device_get_sysattr_value(parent, "manufacturer", &vendor);
            sd_device_get_sysattr_value(parent, "product", &product);
            sd_device_get_sysattr_value(parent, "serial", &serial);

            description = "(";
            if (vendor)  description += string(vendor) + " ";
            if (product) description += string(product) + " ";
            if (serial)  description += string(serial);

            if (!description.empty() && description.back() == ' ')
                description.pop_back();

            description += ")";
        }
        else
        {
            description = " (non-USB)";
        }

        ports.emplace_back(SerialPortEntry{
            .deviceNode = devnodeStr,
            .description = description
        });
    }

    sd_device_enumerator_unref(enumerator);
    return ports;
}