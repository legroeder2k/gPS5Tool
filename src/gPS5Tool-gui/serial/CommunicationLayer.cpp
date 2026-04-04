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

#include "CommunicationLayer.h"

#include <format>
#include <glibmm/dispatcher.h>
#include "SerialPort.h"

CommunicationLayer::CommunicationLayer(Glib::Dispatcher& dispatcher) : _dispatcher(dispatcher) { }

CommunicationLayer::~CommunicationLayer()
{
    disconnect();
}

void CommunicationLayer::connect(const SerialPortEntry& entry)
{
    disconnect();
    auto port = std::make_unique<SerialPort>(entry);
    port->connect();
    port->configurePort();

    _port = std::move(port);
    _running = true;
    _worker = std::thread(&CommunicationLayer::workerLoop, this);

    emitEvent(DeviceEvent{
        .type = DeviceEventType::Connected,
        .text = entry.deviceNode,
        .lines = {}
    });
}

void CommunicationLayer::disconnect()
{
    _running = false;

    if (_worker.joinable())
        _worker.join();

    if (_port)
        _port->disconnect();

    {
        std::lock_guard lock(_txMutex);
        std::queue<OutgoingCommand> empty;
        std::swap(_txQueue, empty);
    }

    _pending.reset();
    _errorLogPostion = 0;

    emitEvent(DeviceEvent{
        .type = DeviceEventType::Disconnected,
        .text = "",
        .lines = {}
    });
}

void CommunicationLayer::workerLoop()
{
    while (_running)
    {
        try
        {
            tryStartNextCommand();

            if (!_port)
                continue;

            auto line = _port->readLine(100);
            if (!line)
                continue;

            handleIncomingLine(*line);
        }
        catch (const std::exception& ex)
        {
            emitEvent(DeviceEvent{
                .type = DeviceEventType::Error,
                .text = ex.what(),
                .lines = {}
            });
        }
    }
}

void CommunicationLayer::queueCommand(const OutgoingCommand& command)
{
    std::lock_guard lock(_txMutex);
    _txQueue.push(command);
}

void CommunicationLayer::tryStartNextCommand()
{
    if (_pending.has_value() || !_port)
        return;

    std::optional<OutgoingCommand> next;

    {
        std::lock_guard lock(_txMutex);
        if (_txQueue.empty())
            return;

        next = _txQueue.front();
        _txQueue.pop();
    }

    if (next->commandType == CommandType::ReadErrorCodes)
        next->text = std::format("errlog {}", _errorLogPostion.load());

    _port->setEcmMode(next->inEcmMode);
    _port->send(next->text);

    _pending = PendingCommand{
        .commandType = next->commandType,
        .text = next->text,
        .expectAdditionalLines = next->expectAdditionalLines,
        .waitingForAdditionalLines = false,
        .replyLines = {}
    };

    emitEvent(DeviceEvent{
        .type = DeviceEventType::SentCommand,
        .text = next->text,
        .lines = {}
    });
}

void CommunicationLayer::handleIncomingLine(const std::string& line)
{
    if (line.starts_with("$$"))
    {
        handleNotification(line);
        return;
    }

    if (line.starts_with("OK") || line.starts_with("NG"))
    {
        handleReplyLine(line);
        return;
    }

    handleUnknownLine(line);
}

void CommunicationLayer::handleNotification(const std::string& line)
{
    emitEvent(DeviceEvent{
        .type = DeviceEventType::Notification,
        .text = line,
        .lines = {}
    });
}

void CommunicationLayer::handleReplyLine(const std::string& line)
{
    if (!_pending.has_value())
    {
        emitEvent(DeviceEvent{
            .type = DeviceEventType::RawLine,
            .text = "Unexpected reply: " + line,
            .lines = {}
        });

        return;
    }

    _pending->replyLines.push_back(line);

    if (_pending->expectAdditionalLines)
    {
        _pending->expectAdditionalLines = false;
        _pending->waitingForAdditionalLines = true;
        return;
    }

    finishPendingCommand();
}

void CommunicationLayer::handleUnknownLine(const std::string& line)
{
    if (_pending.has_value() && _pending->waitingForAdditionalLines)
    {
        _pending->replyLines.push_back(line);
        _pending->waitingForAdditionalLines = false;
        finishPendingCommand();

        return;
    }

    emitEvent(DeviceEvent{
        .type = DeviceEventType::RawLine,
        .text = line,
        .lines = {}
    });
}

void CommunicationLayer::finishPendingCommand()
{
    if (!_pending.has_value())
        return;

    if (_pending->commandType == CommandType::ReadErrorCodes)
    {
        emitEvent(DeviceEvent{
        .type = DeviceEventType::ErrorCodeReply,
        .text = "",
        .lines = _pending->replyLines});
    } else {
        emitEvent(DeviceEvent{
            .type = DeviceEventType::Reply,
            .text = "",
            .lines = _pending->replyLines
        });
    }

    processCompletedReply(*_pending);
    _pending.reset();
}

void CommunicationLayer::processCompletedReply(const PendingCommand& command)
{
    if (command.replyLines.empty())
        return;

    const auto& first = command.replyLines[0];

    if (command.commandType == CommandType::ReadErrorCodes && first.starts_with("OK ") && _errorLogPostion.load() < 39)
    {
        // increment our counter...
        _errorLogPostion.fetch_add(1);

        // line ist like this: OK 00000000 FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFF FFFF FFFF FFFF
        // extract the error code (OK 00000000 <ERRORCODE> <DATA>...) from the line
        const auto errorCode = first.length() > 20 ? first.substr(12, 8) : "FFFFFFFF";
        if (errorCode != "FFFFFFFF")
        {
                queueCommand({
                .commandType = CommandType::ReadErrorCodes,
                .text = "",
                .expectAdditionalLines = false,
                .inEcmMode = true
            });
        }
    }
}

void CommunicationLayer::emitEvent(DeviceEvent event)
{
    {
        std::lock_guard lock(_eventMutex);
        _eventQueue.push(std::move(event));
    }

    _dispatcher.emit();
}

std::optional<DeviceEvent> CommunicationLayer::tryPopEvent()
{
    std::lock_guard lock(_eventMutex);
    if (_eventQueue.empty())
        return std::nullopt;

    auto ev = std::move(_eventQueue.front());
    _eventQueue.pop();

    return ev;
}