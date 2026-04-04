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

#include <cstdint>
#include <sqlite3.h>
#include <string>
#include <vector>

class CodeDatabase {
public:
    CodeDatabase();
    ~CodeDatabase();

    void openOrCreateDatabase();
    void closeDatabase();

    [[nodiscard]] bool isDatabaseOpen() const { return _db != nullptr; }
    [[nodiscard]] std::string getLastUpdate() const;
    [[nodiscard]] std::string lookupErrorCode(const std::string& errorCode) const;

    [[nodiscard]] bool updateDatabase() const;

private:
    struct ErrorCodeRecord
    {
        std::string errorCode;
        std::string description;
    };

    [[nodiscard]] static std::string downloadCodeDatabaseXml();
    [[nodiscard]] static std::vector<ErrorCodeRecord> parseCodeDatabaseXml(const std::string& xml);
    [[nodiscard]] static std::string currentDateString();
    void replaceCodes(const std::vector<ErrorCodeRecord>& records) const;

    [[nodiscard]] bool tableExists(const std::string& tableName) const;
    [[nodiscard]] std::string querySingleValue(const std::string& query) const;

    [[nodiscard]] uint32_t databaseVersion() const;


    void createDatabase() const;

private:
    sqlite3* _db;
    constexpr static auto _databasePath = ".local/share/gPS5Tool/codes.db";
};
