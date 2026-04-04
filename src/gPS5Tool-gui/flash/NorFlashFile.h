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

#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

class NorFlashFile
{
public:
    struct NorArea
    {
        size_t offset;
        size_t size;
        std::string description;
    };

    static const std::map<std::string, NorArea> _norAreaMap;

    NorFlashFile();
    explicit NorFlashFile(std::string flashFileName);

    [[nodiscard]] bool hasFlashFile() const { return !_flashFileName.empty(); }

    [[nodiscard]] std::string md5() const;
    [[nodiscard]] std::string boardId() const;
    [[nodiscard]] uint8_t model() const;
    [[nodiscard]] std::string sku() const;
    [[nodiscard]] uint8_t region() const;
    [[nodiscard]] std::string serialNumber() const;
    [[nodiscard]] std::string boardSerial() const;
    [[nodiscard]] std::string kibanId() const;
    [[nodiscard]] std::string currentFirmware() const;
    [[nodiscard]] std::string minimumFirmware() const;
    [[nodiscard]] std::string maximumFirmware() const;
    [[nodiscard]] std::string lanMacAddress() const;
    [[nodiscard]] std::string wifiMacAddress() const;

private:
    void loadFlashFile();
    void closeFlashFile();

    [[nodiscard]] static std::optional<NorArea> findArea(const std::string& areaName);

    [[nodiscard]] uint64_t loadUint64(const NorArea& norArea) const;
    [[nodiscard]] std::string loadString(const NorArea& norArea) const;
    [[nodiscard]] std::string loadFirmwareVersion(const NorArea& norArea) const;

    [[nodiscard]] static std::string uint64ToHexString(uint64_t value, size_t length, char separator = ':');

    [[nodiscard]] std::string calculateMd5(size_t offset, size_t size) const;


private:
    std::string _flashFileName;
    std::vector<uint8_t> _flashFile;
};
