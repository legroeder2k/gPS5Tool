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

#pragma once

#include <gtkmm.h>

#include "serial/CommunicationLayer.h"
#include "data/CodeDatabase.h"

class SerialPort;

class MainApplication : public Gtk::Application
{
public:
    MainApplication();

private:
    CodeDatabase _codeDatabase;
    Glib::Dispatcher _communicationDispatcher;
    Glib::RefPtr<Gtk::Builder> _builder = nullptr;
    Gtk::ApplicationWindow* _window = nullptr;
    Glib::RefPtr<Gtk::TextBuffer> _logBuffer = nullptr;
    Glib::RefPtr<Gtk::Button> _connectButton = nullptr;

    Glib::RefPtr<Gtk::Button> _readCodesButton = nullptr;
    Glib::RefPtr<Gtk::Button> _clearCodesButton = nullptr;
    Glib::RefPtr<Gtk::Button> _updateErrorCodesButton = nullptr;

    Glib::RefPtr<Gtk::Label> _dbStatusLabel = nullptr;
    std::unique_ptr<CommunicationLayer> _serialComm;

protected:
    void on_activate() override;
    void on_communication_event() const;
    void on_connect_clicked() const;
    void on_read_codes_clicked() const;
    void on_clear_codes_clicked() const;
    void on_update_error_codes_clicked() const;
};
