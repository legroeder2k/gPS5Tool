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


#include "MainApplication.h"

#include <format>

#include "PortChooser.h"

using namespace Gtk;
using namespace Glib;

void MainApplication::on_activate()
{
    _builder = Builder::create_from_resource("/rocks/legroeder/gPS5Tool/ui/mainWindow.ui");
    _window = _builder->get_widget<ApplicationWindow>("main_window");
    _serialComm = std::make_unique<CommunicationLayer>(_communicationDispatcher);

    _communicationDispatcher.connect(sigc::mem_fun(*this, &MainApplication::on_communication_event));

    if (!_window)
        throw std::runtime_error("Could not load main_window!");

    _logBuffer = _builder->get_object<TextBuffer>("log_buffer");
    _connectButton = _builder->get_object<Button>("connect_button");
    _readCodesButton = _builder->get_object<Button>("read_codes");
    _clearCodesButton = _builder->get_object<Button>("clear_codes");
    _updateErrorCodesButton = _builder->get_object<Button>("update_codes");
    _dbStatusLabel = _builder->get_object<Label>("db_status_label");

    _logBuffer->set_text("");
    _readCodesButton->set_sensitive(false);
    _clearCodesButton->set_sensitive(false);

    _connectButton->signal_clicked().connect(sigc::mem_fun(*this, &MainApplication::on_connect_clicked));
    _readCodesButton->signal_clicked().connect(sigc::mem_fun(*this, &MainApplication::on_read_codes_clicked));
    _clearCodesButton->signal_clicked().connect(sigc::mem_fun(*this, &MainApplication::on_clear_codes_clicked));
    _updateErrorCodesButton->signal_clicked().connect(sigc::mem_fun(*this, &MainApplication::on_update_error_codes_clicked));

    _dbStatusLabel->set_text("No code database found!");

    add_window(*_window);

    _window->signal_hide().connect([this]() { delete _window; });

    _window->present();
}

void MainApplication::on_communication_event() const
{
    while (auto event = _serialComm->tryPopEvent())
    {
        switch (event->type)
        {
            case DeviceEventType::Connected:
                _readCodesButton->set_sensitive(true);
                _clearCodesButton->set_sensitive(true);
                _connectButton->set_label(std::format("[{}] Disconnect", event->text));
                _logBuffer->set_text(std::format("Connected to {}\n", event->text));
                break;
            case DeviceEventType::Disconnected:
                _readCodesButton->set_sensitive(false);
                _clearCodesButton->set_sensitive(false);
                _connectButton->set_label("Connect");
                _logBuffer->insert_at_cursor("Disconnected\n");
                break;
            case DeviceEventType::SentCommand:
                _logBuffer->insert_at_cursor(std::format(">> {}\n", event->text));
                break;
            case DeviceEventType::Notification:
            case DeviceEventType::RawLine:
                _logBuffer->insert_at_cursor(std::format("<< {}\n", event->text));
                break;
            case DeviceEventType::Reply:
                for (const auto& line : event->lines)
                    _logBuffer->insert_at_cursor(std::format("{}\n", line));
                break;
            case DeviceEventType::ErrorCodeReply:
                for (const auto& line : event->lines)
                    _logBuffer->insert_at_cursor(std::format("->>> {}\n", line));
                break;
            case DeviceEventType::Error:
                _logBuffer->insert_at_cursor(std::format("Error: {}\n", event->text));
                break;
        }
    }
}

void MainApplication::on_connect_clicked() const
{
    if (_serialComm->isConnected())
    {
        _serialComm->disconnect();
    } else
    {
        if (PortChooser chooser; chooser.run(_window))
        {
            const auto& port = chooser.selectedPort();
            if (!port.has_value())
                return;

            try {
                _serialComm->connect(*port);
            } catch (const std::exception& e) {
                _logBuffer->set_text(e.what());
            }
        }
    }
}

void MainApplication::on_read_codes_clicked() const
{
    if (_serialComm->isConnected())
    {
        _serialComm->queueCommand({
            .commandType = CommandType::ReadErrorCodes,
            .expectAdditionalLines = false,
            .inEcmMode = true
        });
    }
}

void MainApplication::on_clear_codes_clicked() const
{
    if (_serialComm->isConnected())
    {
        _serialComm->queueCommand({
            .commandType = CommandType::SingleCommand,
            .text = "errlog clear",
            .expectAdditionalLines = false,
            .inEcmMode = true
        });
    }
}

void MainApplication::on_update_error_codes_clicked() const
{

}
