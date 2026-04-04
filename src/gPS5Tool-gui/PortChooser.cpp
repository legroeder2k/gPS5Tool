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

#include "PortChooser.h"

#include <format>
#include <stdexcept>

using namespace Gtk;
using namespace Glib;

namespace
{
constexpr guint invalidSelection = GTK_INVALID_LIST_POSITION;
}

PortChooser::PortChooser()
{
    _builder = Builder::create_from_resource("/rocks/legroeder/gPS5Tool/ui/portChooser.ui");
    _dialog = _builder->get_widget<Dialog>("port_chooser");
    _connectButton = _builder->get_widget<Button>("connect_button");
    _cancelButton = _builder->get_widget<Button>("cancel_button");
    _portsList = _builder->get_widget<ListView>("ports_list");

    if (!_dialog || !_connectButton || !_cancelButton || !_portsList)
        throw std::runtime_error("Could not load port chooser widgets");

    _portsModel = StringList::create();
    _selectionModel = SingleSelection::create(_portsModel);
    _factory = SignalListItemFactory::create();

    _factory->signal_setup().connect([](const RefPtr<ListItem>& item)
    {
        const auto label = Gtk::make_managed<Label>();
        label->set_halign(Align::START);
        label->set_margin_start(6);
        label->set_margin_end(6);
        label->set_margin_top(6);
        label->set_margin_bottom(6);
        item->set_child(*label);
    });

    _factory->signal_bind().connect([](const RefPtr<ListItem>& item)
    {
        const auto label = dynamic_cast<Label*>(item->get_child());
        const auto stringObject = std::dynamic_pointer_cast<StringObject>(item->get_item());
        if (!label || !stringObject)
            return;

        label->set_text(stringObject->get_string());
    });

    _portsList->set_factory(_factory);
    _portsList->set_model(_selectionModel);
    _portsList->set_single_click_activate(true);

    _selectionModel->signal_selection_changed().connect([this](guint, guint)
    {
        _connectButton->set_sensitive(_selectionModel->get_selected() != invalidSelection);
    });

    _portsList->signal_activate().connect([this](const guint position)
    {
        _selectionModel->set_selected(position);
        onConnectClicked();
    });

    _connectButton->signal_clicked().connect(sigc::mem_fun(*this, &PortChooser::onConnectClicked));
    _cancelButton->signal_clicked().connect(sigc::mem_fun(*this, &PortChooser::onCancelClicked));
    _dialog->signal_close_request().connect([this]()
    {
        finish(true);
        return true;
    }, false);
    _dialog->signal_hide().connect(sigc::mem_fun(*this, &PortChooser::onDialogHide));

    populatePorts();
}

PortChooser::~PortChooser()
{
    delete _dialog;
}

bool PortChooser::run(Window* parent)
{
    _cancelled = true;
    _selectedPort.reset();

    populatePorts();

    if (parent)
    {
        _dialog->set_transient_for(*parent);
    }

    _dialog->set_modal(true);
    _dialog->present();

    _mainLoop = MainLoop::create(false);
    _mainLoop->run();
    _mainLoop.reset();

    return !_cancelled;
}

void PortChooser::populatePorts()
{
    _ports = SerialPort::getSerialDevices();
    _portsModel->splice(0, _portsModel->get_n_items(), std::vector<Glib::ustring>{});

    for (const auto& [deviceNode, description] : _ports)
    {
        _portsModel->append(std::format("{}: {}", deviceNode, description));
    }

    if (_ports.empty())
    {
        _selectionModel->set_selected(invalidSelection);
        _connectButton->set_sensitive(false);
        return;
    }

    _selectionModel->set_selected(0);
    _connectButton->set_sensitive(true);
}

void PortChooser::finish(const bool cancelled)
{
    _cancelled = cancelled;

    if (!cancelled)
    {
        if (const auto selectedIndex = _selectionModel->get_selected(); selectedIndex == invalidSelection || selectedIndex >= _ports.size())
        {
            _cancelled = true;
            _selectedPort.reset();
        }
        else
        {
            _selectedPort = _ports[selectedIndex];
        }
    }
    else
    {
        _selectedPort.reset();
    }

    _dialog->hide();
}

void PortChooser::onConnectClicked()
{
    finish(false);
}

void PortChooser::onCancelClicked()
{
    finish(true);
}

void PortChooser::onDialogHide() const
{
    if (_mainLoop && _mainLoop->is_running())
        _mainLoop->quit();
}
