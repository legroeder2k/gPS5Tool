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

#include "CodeDatabase.h"

#include <curl/curl.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#define CURRENT_VERSION 1

using namespace std;

namespace
{
size_t writeToString(const char* ptr, const size_t size, const size_t nmemb, void* userdata)
{
    auto* buffer = static_cast<string*>(userdata);
    buffer->append(ptr, size * nmemb);
    return size * nmemb;
}

string trim(const string& value)
{
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == string::npos)
        return "";

    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

string decodeXmlEntities(string value)
{
    const pair<string_view, string_view> entities[] = {
        {"&amp;", "&"},
        {"&lt;", "<"},
        {"&gt;", ">"},
        {"&quot;", "\""},
        {"&apos;", "'"}
    };

    for (const auto& [entity, replacement] : entities)
    {
        size_t offset = 0;
        while ((offset = value.find(entity, offset)) != string::npos)
        {
            value.replace(offset, entity.size(), replacement);
            offset += replacement.size();
        }
    }

    return value;
}

string extractTagValue(const string& xml, const string_view tag, const size_t offset = 0)
{
    const string openTag = "<" + string(tag) + ">";
    const string closeTag = "</" + string(tag) + ">";

    const auto open = xml.find(openTag, offset);
    if (open == string::npos)
        return "";

    const auto valueStart = open + openTag.size();
    const auto close = xml.find(closeTag, valueStart);
    if (close == string::npos)
        return "";

    return decodeXmlEntities(trim(xml.substr(valueStart, close - valueStart)));
}
}

CodeDatabase::CodeDatabase()
{
    // First we need to expand the database path (replace ~ with the users home)

    // Make sure the database path exists
    if (const auto dbPath = filesystem::path(getenv("HOME")) / _databasePath; !filesystem::exists(dbPath.parent_path()))
    {
        filesystem::create_directories(dbPath.parent_path());
    }

    _db = nullptr;
}

CodeDatabase::~CodeDatabase()
{
    closeDatabase();
}

void CodeDatabase::openOrCreateDatabase()
{
    const auto dbPath = filesystem::path(getenv("HOME")) / _databasePath;
    sqlite3_open(dbPath.string().c_str(), &_db);

    // check if our version table exists
    if (!tableExists("settings"))
    {
        createDatabase();
    }

    if (const auto dbVersion = databaseVersion(); dbVersion < CURRENT_VERSION)
    {
        // TODO: Need to upgrade database
    }
}

void CodeDatabase::closeDatabase()
{
    if (isDatabaseOpen())
    {
        sqlite3_close(_db);
        _db = nullptr;
    }
}

string CodeDatabase::getLastUpdate() const
{
    return querySingleValue("SELECT value FROM settings WHERE name='last_update'");
}

string CodeDatabase::lookupErrorCode(const string& errorCode) const
{
    return querySingleValue("SELECT description FROM codes WHERE errorcode='" + errorCode + "'");
}

bool CodeDatabase::updateDatabase() const
{
    if (!isDatabaseOpen())
        return false;

    const auto xml = downloadCodeDatabaseXml();
    const auto records = parseCodeDatabaseXml(xml);
    if (records.empty())
        return false;

    replaceCodes(records);
    return true;
}

bool CodeDatabase::tableExists(const string& tableName) const
{
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(_db, "SELECT name FROM sqlite_master WHERE type='table' AND name=?", -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return true;
    }
    sqlite3_finalize(stmt);

    return false;
}

string CodeDatabase::querySingleValue(const std::string& query) const
{
    sqlite3_stmt* stmt = nullptr;
    if (const int rc = sqlite3_prepare_v2(_db, query.c_str(), -1, &stmt, nullptr); rc != SQLITE_OK)
        return "";

    if (const int rc = sqlite3_step(stmt); rc != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return "";
    }

    const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    const string result = value ? value : "";
    sqlite3_finalize(stmt);
    return result;
}

uint32_t CodeDatabase::databaseVersion() const
{
    return strtoul(querySingleValue("SELECT value FROM settings WHERE name='version'").c_str(), nullptr, 10);
}

void CodeDatabase::createDatabase() const
{
    const auto sql =    "CREATE TABLE settings (name TEXT PRIMARY KEY, value TEXT);"
                        "CREATE TABLE codes (errorcode TEXT PRIMARY KEY, description TEXT);"
                        "INSERT INTO settings (name, value) VALUES ('version', '1');"
                        "INSERT INTO settings (name, value) VALUES ('last_update', '0');";
    const char* tail = sql;

    while (*tail)
    {
        sqlite3_stmt *stmt;
        if (const auto rc = sqlite3_prepare_v2(_db, tail, -1, &stmt, &tail); rc != SQLITE_OK)
        {
            throw runtime_error("Could not prepare SQL statement: " + string(tail));
        }

        if (!stmt) continue;

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

string CodeDatabase::downloadCodeDatabaseXml()
{
    const auto curl = curl_easy_init();
    if (!curl)
        throw runtime_error("Could not initialize CURL");

    string response;
    curl_easy_setopt(curl, CURLOPT_URL, "https://uartcodes.com/xml.php");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "gPS5Tool/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    if (const auto rc = curl_easy_perform(curl); rc != CURLE_OK)
    {
        const auto error = string(curl_easy_strerror(rc));
        curl_easy_cleanup(curl);
        throw runtime_error("Could not download XML database: " + error);
    }

    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    curl_easy_cleanup(curl);

    if (responseCode != 200)
        throw runtime_error("Unexpected HTTP response code: " + to_string(responseCode));

    return response;
}

vector<CodeDatabase::ErrorCodeRecord> CodeDatabase::parseCodeDatabaseXml(const string& xml)
{
    vector<ErrorCodeRecord> records;
    constexpr string_view openEntry = "<errorCode>";
    constexpr string_view closeEntry = "</errorCode>";

    size_t offset = 0;
    while (true)
    {
        const auto entryStart = xml.find(openEntry, offset);
        if (entryStart == string::npos)
            break;

        const auto contentStart = entryStart + openEntry.size();
        const auto entryEnd = xml.find(closeEntry, contentStart);
        if (entryEnd == string::npos)
            break;

        const auto entryXml = xml.substr(contentStart, entryEnd - contentStart);
        auto errorCode = extractTagValue(entryXml, "ErrorCode");
        if (auto description = extractTagValue(entryXml, "Description"); !errorCode.empty() && !description.empty())
        {
            records.emplace_back(ErrorCodeRecord{
                .errorCode = std::move(errorCode),
                .description = std::move(description)
            });
        }

        offset = entryEnd + closeEntry.size();
    }

    return records;
}

string CodeDatabase::currentDateString()
{
    const auto now = chrono::system_clock::now();
    const auto time = chrono::system_clock::to_time_t(now);
    const auto localTime = *std::localtime(&time);

    ostringstream date;
    date << put_time(&localTime, "%Y-%m-%d");
    return date.str();
}

void CodeDatabase::replaceCodes(const vector<ErrorCodeRecord>& records) const
{
    char* errorMessage = nullptr;
    if (sqlite3_exec(_db, "BEGIN IMMEDIATE TRANSACTION", nullptr, nullptr, &errorMessage) != SQLITE_OK)
    {
        const string error = errorMessage ? errorMessage : "Unknown SQLite error";
        sqlite3_free(errorMessage);
        throw runtime_error("Could not start database transaction: " + error);
    }

    auto rollbackTransaction = [this]()
    {
        sqlite3_exec(_db, "ROLLBACK", nullptr, nullptr, nullptr);
    };

    sqlite3_stmt* deleteStmt = nullptr;
    sqlite3_stmt* insertStmt = nullptr;
    sqlite3_stmt* updateStmt = nullptr;

    try
    {
        if (sqlite3_prepare_v2(_db, "DELETE FROM codes", -1, &deleteStmt, nullptr) != SQLITE_OK)
            throw runtime_error("Could not prepare codes cleanup statement");
        if (sqlite3_step(deleteStmt) != SQLITE_DONE)
            throw runtime_error("Could not clear codes table");
        sqlite3_finalize(deleteStmt);
        deleteStmt = nullptr;

        if (sqlite3_prepare_v2(_db, "INSERT INTO codes (errorcode, description) VALUES (?, ?)", -1, &insertStmt, nullptr) != SQLITE_OK)
            throw runtime_error("Could not prepare code insert statement");

        for (const auto& record : records)
        {
            sqlite3_reset(insertStmt);
            sqlite3_clear_bindings(insertStmt);
            sqlite3_bind_text(insertStmt, 1, record.errorCode.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insertStmt, 2, record.description.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(insertStmt) != SQLITE_DONE)
                throw runtime_error("Could not insert code entry");
        }

        sqlite3_finalize(insertStmt);
        insertStmt = nullptr;

        if (sqlite3_prepare_v2(_db, "UPDATE settings SET value=? WHERE name='last_update'", -1, &updateStmt, nullptr) != SQLITE_OK)
            throw runtime_error("Could not prepare last_update statement");

        const auto currentDate = currentDateString();
        sqlite3_bind_text(updateStmt, 1, currentDate.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(updateStmt) != SQLITE_DONE)
            throw runtime_error("Could not update last_update");

        sqlite3_finalize(updateStmt);
        updateStmt = nullptr;

        if (sqlite3_exec(_db, "COMMIT", nullptr, nullptr, &errorMessage) != SQLITE_OK)
        {
            const string error = errorMessage ? errorMessage : "Unknown SQLite error";
            sqlite3_free(errorMessage);
            throw runtime_error("Could not commit database transaction: " + error);
        }
    }
    catch (...)
    {
        if (deleteStmt)
            sqlite3_finalize(deleteStmt);
        if (insertStmt)
            sqlite3_finalize(insertStmt);
        if (updateStmt)
            sqlite3_finalize(updateStmt);
        rollbackTransaction();
        throw;
    }
}
