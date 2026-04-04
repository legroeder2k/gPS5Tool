// Copyright (c) 2026, Sascha Huck <me@legroeder.rocks>
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

#pragma once
#include <string>
#include <vector>
#include <optional>

struct SerialPortEntry
{
        std::string deviceNode;
        std::string description;
};

class SerialPort {
public:
    SerialPort();
    explicit SerialPort(const SerialPortEntry& serialPort);
    ~SerialPort();

    [[nodiscard]] bool isConnected() const { return _fd >= 0; }

    void connect();
    void disconnect();

    void configurePort() const;
    void setEcmMode(bool ecmMode);
    void send(const std::string& data) const;

    [[nodiscard]] bool waitForData(int timeoutMs) const;
    void readAvailable();
    bool tryExtractLine(std::string &outLine);
    std::optional<std::string> readLine(int timeoutMs);

    static std::vector<SerialPortEntry> getSerialDevices();

private:
    std::string _deviceName;
    int _fd = -1;
    bool _ecmMode = false;

    std::string _readBuffer;
};
