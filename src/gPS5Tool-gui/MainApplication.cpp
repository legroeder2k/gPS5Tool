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


#include "MainApplication.h"

#include <format>

#include "PortChooser.h"

using namespace Gtk;
using namespace Glib;

MainApplication::MainApplication() : Application("rocks.legroeder.gPS5Tool"), _codeDatabase(CodeDatabase()), _norFlashFile(NorFlashFile())
{
    _codeDatabase.openOrCreateDatabase();
}

void MainApplication::on_activate()
{
    /* Create the window from the .ui file */
    _builder = Builder::create_from_resource("/rocks/legroeder/gPS5Tool/ui/mainWindow.ui");
    _window = _builder->get_widget<ApplicationWindow>("main_window");
    if (!_window)
        throw std::runtime_error("Could not load main_window!");

    /* File filters */
    _txtFileFilter = FileFilter::create();
    _txtFileFilter->set_name("Text files");
    _txtFileFilter->add_suffix("txt");

    _logFileFilter = FileFilter::create();
    _logFileFilter->set_name("Log files");
    _logFileFilter->add_suffix("log");

    _allFileFilter = FileFilter::create();
    _allFileFilter->set_name("All files");
    _allFileFilter->add_pattern("*");

    _storeErrorLogFilters = Gio::ListStore<FileFilter>::create();
    _storeErrorLogFilters->append(_txtFileFilter);
    _storeErrorLogFilters->append(_logFileFilter);
    _storeErrorLogFilters->append(_allFileFilter);

    _norFileFilter = FileFilter::create();
    _norFileFilter->set_name("NOR Flash files");
    _norFileFilter->add_suffix("bin");

    _openNorFileFilters = Gio::ListStore<FileFilter>::create();
    _openNorFileFilters->append(_norFileFilter);
    _openNorFileFilters->append(_allFileFilter);

    /* All the stuff for Serial Communication */
    _serialComm = std::make_unique<CommunicationLayer>(_communicationDispatcher);
    _communicationDispatcher.connect(sigc::mem_fun(*this, &MainApplication::on_communication_event));

    _logBuffer = _builder->get_object<TextBuffer>("log_buffer");
    _logView = _builder->get_object<TextView>("log_view");
    _connectButton = _builder->get_object<Button>("connect_button");
    _readCodesButton = _builder->get_object<Button>("read_codes");
    _clearCodesButton = _builder->get_object<Button>("clear_codes");
    _saveErrorLogButton = _builder->get_object<Button>("save_error_log");
    _clearErrorLogButton = _builder->get_object<Button>("clear_error_log");
    _updateErrorCodesButton = _builder->get_object<Button>("update_codes");
    _dbStatusLabel = _builder->get_object<Label>("db_status_label");

    _logBuffer->set_text("");
    toggleUartButtons(false);

    _connectButton->signal_clicked().connect(sigc::mem_fun(*this, &MainApplication::on_connect_clicked));
    _readCodesButton->signal_clicked().connect(sigc::mem_fun(*this, &MainApplication::on_read_codes_clicked));
    _clearCodesButton->signal_clicked().connect(sigc::mem_fun(*this, &MainApplication::on_clear_codes_clicked));
    _saveErrorLogButton->signal_clicked().connect(sigc::mem_fun(*this, &MainApplication::on_save_error_log_clicked));
    _clearErrorLogButton->signal_clicked().connect(sigc::mem_fun(*this, &MainApplication::on_clear_error_log_clicked));
    _updateErrorCodesButton->signal_clicked().connect(sigc::mem_fun(*this, &MainApplication::on_update_error_codes_clicked));

    if (const auto lastUpdate = _codeDatabase.getLastUpdate(); lastUpdate == "0")
        _dbStatusLabel->set_text("No code database found!");
    else
        _dbStatusLabel->set_text("Last database update: " + lastUpdate);

    /* All the stuff for NOR Flash */
    _openFlashFileButton = _builder->get_object<Button>("open_flash_file");

    _norMd5 = _builder->get_object<TextBuffer>("nor_md5");
    _norBoardId = _builder->get_object<TextBuffer>("nor_board_id");
    _norModelCombo = _builder->get_object<ComboBox>("nor_model_combo");
    _norSku = _builder->get_object<TextBuffer>("nor_sku");
    _norSerial = _builder->get_object<TextBuffer>("nor_serial");
    _norBoardSerial = _builder->get_object<TextBuffer>("nor_board_serial");
    _norKibanId = _builder->get_object<TextBuffer>("nor_kiban_id");
    _norCurrentFirmware = _builder->get_object<TextBuffer>("nor_current_fw");
    _norFirmwareLimits = _builder->get_object<TextBuffer>("nor_fw_limits");
    _norWifiMac = _builder->get_object<TextBuffer>("nor_wlan_mac");
    _norLanMac = _builder->get_object<TextBuffer>("nor_lan_mac");
    _norRegionCombo = _builder->get_object<ComboBox>("nor_region_combo");

    _norStatusLabel = _builder->get_object<Label>("nor_status_label");

    _openFlashFileButton->signal_clicked().connect(sigc::mem_fun(*this, &MainApplication::on_openFlashFile_clicked));

    createComboBoxModel();

    /* Make this the application window */
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
                toggleUartButtons(true);
                _connectButton->set_label(std::format("[{}] Disconnect", event->text));
                _logBuffer->set_text(std::format("Connected to {}\n", event->text));
                break;
            case DeviceEventType::Disconnected:
                toggleUartButtons(false);
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
                    _logBuffer->insert_at_cursor(std::format("<< {}\n", line));
                break;
            case DeviceEventType::ErrorCodeReply:
                {
                    const auto& first = event->lines.front();
                    _logBuffer->insert_at_cursor(std::format("<< {}\n", first));

                    const auto& errorCode = first.length() > 20 ? first.substr(12, 8) : "FFFFFFFF";
                    if (errorCode != "FFFFFFFF")
                    {
                        auto errorText = _codeDatabase.lookupErrorCode(errorCode);
                        if (errorText.empty())
                            errorText = "Unknown error code";

                        _logBuffer->insert_at_cursor(std::format("  {}\n", errorText));
                    }
                    break;
                }
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
                _logBuffer->set_text(e.what() + std::string("\n"));
            }
        }
    }
}

void MainApplication::on_read_codes_clicked() const
{
    if (_serialComm->isConnected())
    {
        _serialComm->resetErrorCodeCounter();

        _serialComm->queueCommand({
            .commandType = CommandType::ReadErrorCodes,
            .text = "",
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

void MainApplication::on_save_error_log_clicked()
{
    if (_logBuffer->get_text().empty())
        return;

    if (!_fileDialog)
    {
        _fileDialog = Gtk::FileDialog::create();
        _fileDialog->set_modal(true);
    }

    _fileDialog->set_filters(_storeErrorLogFilters);
    _fileDialog->set_default_filter(_txtFileFilter);
    _fileDialog->save(*_window, sigc::mem_fun(*this, &MainApplication::on_save_error_log_fileSelected));
}

void MainApplication::on_clear_error_log_clicked()
{
    _logBuffer->set_text("");
}

void MainApplication::on_update_error_codes_clicked() const
{
    try {
    if (_codeDatabase.updateDatabase())
        _dbStatusLabel->set_text("SUCCESS - Last database update: " + _codeDatabase.getLastUpdate());
    } catch (const std::exception& e)
    {
        _dbStatusLabel->set_text(std::string("Error updating database: ") + e.what());
    }
}

void MainApplication::on_save_error_log_fileSelected(const RefPtr<Gio::AsyncResult>& result) const
{
    RefPtr<Gio::File> file;

    try
    {
        file = _fileDialog->save_finish(result);
    } catch (const DialogError&)
    {
        return;
    }

    const auto selectedFile = file->get_path();

    try
    {
        // Get the buffer from _logBuffer and store it to selectedFile
        const auto buffer = _logBuffer->get_text();
        const auto size = strlen(buffer.c_str());
        gsize written = 0;
        file->replace()->write_all(buffer.c_str(), size, written);

        const auto success = AlertDialog::create();
        success->set_message("Error log saved successfully to: " + selectedFile);
        success->show(*_window);
    } catch (const Error& err)
    {
        const auto error = AlertDialog::create();
        error->set_message("Error saving log");
        error->set_detail(err.what());
        error->show(*_window);
    }

}

void MainApplication::toggleUartButtons(const bool enabled) const
{
    _readCodesButton->set_sensitive(enabled);
    _clearCodesButton->set_sensitive(enabled);
}

void MainApplication::createComboBoxModel()
{
    {
        _norModelItems = ListStore::create(_modelComboColumns);
        _norModelCombo->set_model(_norModelItems);

        const auto iter = _norModelItems->append();
        auto row = *iter;
        row[_modelComboColumns.m_col_id] = 0x89;
        row[_modelComboColumns.m_col_name] = "Disc Edition";

        row = *_norModelItems->append();
        row[_modelComboColumns.m_col_id] = 0x8A;
        row[_modelComboColumns.m_col_name] = "Digital Edition";

        _norModelCombo->pack_start(_modelComboColumns.m_col_name, true);
    }

    {
        _norRegionItems = ListStore::create(_regionComboColumns);
        _norRegionCombo->set_model(_norRegionItems);
        const auto iter = _norRegionItems->append();
        auto row = *iter;
        row[_regionComboColumns.m_col_id] = 0;
        row[_regionComboColumns.m_col_region] = "[00] Japan";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 1;
        row[_regionComboColumns.m_col_region] = "[01] US, Canada (North America)";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 2;
        row[_regionComboColumns.m_col_region] = "[02] Australia / New Zealand";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 3;
        row[_regionComboColumns.m_col_region] = "[03] U.K. / Ireland";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 4;
        row[_regionComboColumns.m_col_region] = "[04] Europe / Middle East / Africa";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 5;
        row[_regionComboColumns.m_col_region] = "[05] Korea (South Korea)";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 6;
        row[_regionComboColumns.m_col_region] = "[06] Southeast Asia / Hong Kong";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 7;
        row[_regionComboColumns.m_col_region] = "[07] Taiwan";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 8;
        row[_regionComboColumns.m_col_region] = "[08] Russia, Ukraine, India, Central Asia";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 9;
        row[_regionComboColumns.m_col_region] = "[09] Mainland China";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 10;
        row[_regionComboColumns.m_col_region] = "[10] !! unused !!";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 11;
        row[_regionComboColumns.m_col_region] = "[11] Mexico, Central America, South America";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 12;
        row[_regionComboColumns.m_col_region] = "[12] !! unused !!";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 13;
        row[_regionComboColumns.m_col_region] = "[13] !! unused !!";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 14;
        row[_regionComboColumns.m_col_region] = "[14] Mexico, Central America, South America";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 15;
        row[_regionComboColumns.m_col_region] = "[15] US, Canada (North America)";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 16;
        row[_regionComboColumns.m_col_region] = "[16] Europe / Middle East / Africa";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 17;
        row[_regionComboColumns.m_col_region] = "[17] !! unused !!";

        row = *_norRegionItems->append();
        row[_regionComboColumns.m_col_id] = 18;
        row[_regionComboColumns.m_col_region] = "[18] Singapore, Korea, Asia";

        _norRegionCombo->pack_start(_regionComboColumns.m_col_region, true);
    }
}

void MainApplication::on_openFlashFile_clicked()
{
    if (!_fileDialog)
    {
        _fileDialog = FileDialog::create();
        _fileDialog->set_modal(true);
    }

    _fileDialog->set_filters(_openNorFileFilters);
    _fileDialog->set_default_filter(_norFileFilter);
    _fileDialog->open(*_window, sigc::mem_fun(*this, &MainApplication::on_openFlashFile_fileSelected));
}

void MainApplication::on_openFlashFile_fileSelected(const RefPtr<Gio::AsyncResult>& result)
{
    RefPtr<Gio::File> file;

    try
    {
        file = _fileDialog->open_finish(result);
    } catch (const DialogError&)
    {
        return;
    }

    const auto selectedFile = file->get_path();
    _norStatusLabel->set_text("NOR Flash file: " + selectedFile);
    _norFlashFile = NorFlashFile(selectedFile);
    _norMd5->set_text(_norFlashFile.md5());
    _norBoardId->set_text(_norFlashFile.boardId());
    _norModelCombo->set_active(_norFlashFile.model() == 0x89 ? 0 : 1);
    _norSku->set_text(_norFlashFile.sku());
    _norSerial->set_text(_norFlashFile.serialNumber());
    _norBoardSerial->set_text(_norFlashFile.boardSerial());
    _norRegionCombo->set_active(_norFlashFile.region());
    _norKibanId->set_text(_norFlashFile.kibanId());
    _norCurrentFirmware->set_text(_norFlashFile.currentFirmware());
    _norFirmwareLimits->set_text(std::format("{} / {}", _norFlashFile.minimumFirmware(), _norFlashFile.maximumFirmware()));
    _norLanMac->set_text(_norFlashFile.lanMacAddress());
    _norWifiMac->set_text(_norFlashFile.wifiMacAddress());
}
