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

#include <optional>
#include <vector>

#include <gtkmm.h>

#include "serial/SerialPort.h"

class PortChooser
{
public:
    PortChooser();
    ~PortChooser();

    bool run(Gtk::Window* parent = nullptr);

    [[nodiscard]] bool wasCancelled() const { return _cancelled; }
    [[nodiscard]] const std::optional<SerialPortEntry>& selectedPort() const { return _selectedPort; }

private:
    void populatePorts();
    void finish(bool cancelled);
    void onConnectClicked();
    void onCancelClicked();
    void onDialogHide() const;

    Glib::RefPtr<Gtk::Builder> _builder;
    Gtk::Dialog* _dialog = nullptr;
    Gtk::Button* _connectButton = nullptr;
    Gtk::Button* _cancelButton = nullptr;
    Gtk::ListView* _portsList = nullptr;


    Glib::RefPtr<Gtk::StringList> _portsModel;
    Glib::RefPtr<Gtk::SingleSelection> _selectionModel;
    Glib::RefPtr<Gtk::SignalListItemFactory> _factory;
    Glib::RefPtr<Glib::MainLoop> _mainLoop;

    std::vector<SerialPortEntry> _ports;
    std::optional<SerialPortEntry> _selectedPort;
    bool _cancelled = true;
};
