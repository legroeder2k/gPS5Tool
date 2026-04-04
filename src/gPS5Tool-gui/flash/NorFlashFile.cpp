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

#include "NorFlashFile.h"

#include <format>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <openssl/evp.h>
#include <bit>

using namespace std;

NorFlashFile::NorFlashFile() = default;
NorFlashFile::NorFlashFile(string flashFileName) : _flashFileName(std::move(flashFileName))
{
    loadFlashFile();
}

const map<string, NorFlashFile::NorArea> NorFlashFile::_norAreaMap = {
    {"ACT_SLOT",    {0x001000, 1,   "Active slot"}},

    {"BOARD_ID",    {0x1C4000, 8,   "Board ID"}},
    {"MAC",         {0x1C4020, 6,   "LAN MAC address"}},

    {"MODEL",       {0x1C7011, 1,   "Model variant"}},                          // 01 slim, 02 disc, 03 digital
    {"MODEL2",      {0x1C7038, 1,   "Model variant 2"}},                        // 89 disc, 8D digital

    {"MB_SN",       {0x1C7200, 16,  "Motherboard serial number"}},
    {"SN",          {0x1C7210, 17,  "Console serial"}},
    {"SKU",         {0x1C7230, 13,  "SKU Version"}},
    {"REGION",      {0x1C7236, 2,   "Console region"}},
    {"KIBAN",       {0x1C7250, 13,  "Kiban ID"}},
    {"SOCUID",      {0x1C7260, 16,  "Social UID"}},
    {"WIFIMAC",     {0x1C73C0, 6,   "WIFI MAC address"}},

    {"EAP_MGC",     {0x1C75FC, 4,   "EAP Magic Key"}},

    {"FW_F_U",      {0x1C8068, 4,   "Firmware FW upper"}},

    {"FW_M",        {0x1C8C10, 8,   "Minimum FW"}},
    {"FW_M_TS",     {0x1C8C18, 8,   "Minimum FW timestamp"}},

    {"FW",          {0x1C8C20, 8,   "Current FW"}},
    {"FW_TS",       {0x1C8C28, 8,   "Current FW timestmap"}},

    {"FW_F",        {0x1C8C30, 8,   "Factory FW"}},
    {"FW_XX",       {0x1C8C38, 8,   "Factory FW xx"}},

    {"IDU",         {0x1C9600, 1,   "Kiosk mode"}}

};

void NorFlashFile::loadFlashFile()
{
    if (_flashFileName.empty()) return;

    // Read the contents of _flashFileName into _flashFile
    ifstream file(_flashFileName, ios::binary);
    if (!file) return;

    file.seekg(0, ios::end);
    _flashFile.resize(file.tellg());
    file.seekg(0, ios::beg);
    file.read(reinterpret_cast<char*>(_flashFile.data()), _flashFile.size());
}

void NorFlashFile::closeFlashFile()
{
    _flashFile.clear();
    _flashFileName.clear();
}

string NorFlashFile::md5() const
{
    return calculateMd5(0, _flashFile.size());
}

string NorFlashFile::boardId() const
{
    auto boardId = loadUint64(findArea("BOARD_ID").value());
    return uint64ToHexString(boardId, 8, ':');
}

uint8_t NorFlashFile::model() const
{
    return loadUint64(findArea("MODEL2").value());
}

std::string NorFlashFile::sku() const
{
    return loadString(findArea("SKU").value());
}

uint8_t NorFlashFile::region() const
{
    const auto region = loadString(findArea("REGION").value());
    return static_cast<uint8_t>(std::stoi(region));
}

std::string NorFlashFile::serialNumber() const
{
    return loadString(findArea("SN").value());
}

string NorFlashFile::boardSerial() const
{
    return loadString(findArea("MB_SN").value());
}

std::string NorFlashFile::kibanId() const
{
    return loadString(findArea("KIBAN").value());
}

string NorFlashFile::currentFirmware() const
{
    return loadFirmwareVersion(findArea("FW").value());
}

string NorFlashFile::minimumFirmware() const
{
    return loadFirmwareVersion(findArea("FW_M").value());
}

string NorFlashFile::maximumFirmware() const
{
    return loadFirmwareVersion(findArea("FW_F").value());
}

string NorFlashFile::lanMacAddress() const
{
    auto macInBytes = loadUint64(findArea("MAC").value());
    return uint64ToHexString(macInBytes, 6, ':');
}

string NorFlashFile::wifiMacAddress() const
{
    auto macInBytes = loadUint64(findArea("WIFIMAC").value());
    return uint64ToHexString(macInBytes, 6, ':');
}

optional<NorFlashFile::NorArea> NorFlashFile::findArea(const string& areaName)
{
    if (!_norAreaMap.contains(areaName)) return nullopt;
    return _norAreaMap.at(areaName);
}

uint64_t NorFlashFile::loadUint64(const NorArea& norArea) const
{
    auto readSize = min(sizeof(uint64_t), norArea.size);

    if (norArea.offset + readSize > _flashFile.size()) return 0;

    if (readSize == 1) return _flashFile[norArea.offset];
    if (readSize == 2) return _flashFile[norArea.offset] | (_flashFile[norArea.offset + 1] << 8);

    uint64_t result = 0;
    for (size_t i = 0; i < readSize; ++i) {
        result |= static_cast<uint64_t>(_flashFile[norArea.offset + i]) << (i * 8);
    }

    return result;
}

string NorFlashFile::loadString(const NorArea& norArea) const
{
    if (norArea.offset + norArea.size > _flashFile.size()) return "";
    return { _flashFile.begin() + norArea.offset, _flashFile.begin() + norArea.offset + norArea.size };
}

string NorFlashFile::loadFirmwareVersion(const NorArea& norArea) const
{
    auto firmwareBytes = loadUint64(norArea);
    return format("{:02X}.{:02X}.{:02X}.{:02X}", (firmwareBytes >> 56) & 0xff, (firmwareBytes >> 48) & 0xFF, (firmwareBytes >> 40) & 0xFF, (firmwareBytes >> 32) & 0xFF);
}

string NorFlashFile::uint64ToHexString(uint64_t value, size_t length, char separator)
{
    if (length > 8) length = 8;

    string hexString;
    for (size_t i = 0; i < length; ++i) {
        hexString += std::format("{:02X}", value & 0xFF);
        hexString += separator;
        value >>= 8;
    }

    hexString.pop_back();
    return hexString;
}

std::string NorFlashFile::calculateMd5(const size_t offset, const size_t size) const
{
    // Calculate the MD5 checksum over _flashData starting from offset up until size
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    unsigned char md5[EVP_MAX_MD_SIZE];
    unsigned int md5_len;

    EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
    EVP_DigestUpdate(ctx, _flashFile.data() + offset, size);
    EVP_DigestFinal_ex(ctx, md5, &md5_len);
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (ssize_t i = 0; i < md5_len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(md5[i]);
    }

    return oss.str();
}
