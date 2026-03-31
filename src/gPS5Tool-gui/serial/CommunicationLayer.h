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
#include <condition_variable>
#include <memory>
#include <optional>
#include <queue>
#include <thread>

struct SerialPortEntry;

namespace Glib
{
    class Dispatcher;
}

class SerialPort;

enum class CommandType
{
    SingleCommand,
    ReadErrorCodes,
};

struct OutgoingCommand
{
    CommandType commandType;
    std::string text;
    bool expectAdditionalLines = false;
    bool inEcmMode = false;
};

struct PendingCommand
{
    CommandType commandType;
    std::string text;
    bool expectAdditionalLines = false;
    bool waitingForAdditionalLines = false;
    std::vector<std::string> replyLines;
};

enum class DeviceEventType
{
    Connected,
    Disconnected,
    SentCommand,
    Notification,
    Reply,
    ErrorCodeReply,
    Error,
    RawLine
};

struct DeviceEvent
{
    DeviceEventType type;
    std::string text;
    std::vector<std::string> lines;
};

class CommunicationLayer {
public:
    explicit CommunicationLayer(Glib::Dispatcher& dispatcher);
    ~CommunicationLayer();

    void connect(const SerialPortEntry& entry);
    void disconnect();

    bool isConnected() const { return _running; };

    void queueCommand(const OutgoingCommand& command);

    std::optional<DeviceEvent> tryPopEvent();

private:
    void workerLoop();

    void emitEvent(DeviceEvent event);

    void handleIncomingLine(const std::string& line);
    void handleNotification(const std::string& line);
    void handleReplyLine(const std::string& line);
    void handleUnknownLine(const std::string& line);

    void tryStartNextCommand();
    void finishPendingCommand();
    void processCompletedReply(const PendingCommand& command);

private:
    Glib::Dispatcher& _dispatcher;

    std::unique_ptr<SerialPort> _port;

    std::thread _worker;
    std::atomic<bool> _running{false};

    mutable std::mutex _stateMutex;

    std::mutex _txMutex;
    std::queue<OutgoingCommand> _txQueue;

    std::mutex _eventMutex;
    std::queue<DeviceEvent> _eventQueue;

    std::optional<PendingCommand> _pending;

    std::atomic<uint8_t> _errorLogPostion{0};
};

