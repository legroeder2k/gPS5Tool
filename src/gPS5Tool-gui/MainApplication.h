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

#pragma once

#include <gtkmm.h>

#include "serial/CommunicationLayer.h"
#include "data/CodeDatabase.h"
#include "flash/NorFlashFile.h"

class SerialPort;

class MainApplication : public Gtk::Application
{
public:
    MainApplication();

private:
    /* Window stuff */

    Glib::RefPtr<Gtk::Builder> _builder = nullptr;
    Gtk::ApplicationWindow* _window = nullptr;

    /* Global stuff */
    Glib::RefPtr<Gtk::FileDialog> _fileDialog = nullptr;

    Glib::RefPtr<Gtk::FileFilter> _logFileFilter = nullptr;
    Glib::RefPtr<Gtk::FileFilter> _txtFileFilter = nullptr;
    Glib::RefPtr<Gtk::FileFilter> _allFileFilter = nullptr;
    Glib::RefPtr<Gio::ListStore<Gtk::FileFilter>>  _storeErrorLogFilters = nullptr;

    Glib::RefPtr<Gtk::FileFilter> _norFileFilter = nullptr;
    Glib::RefPtr<Gio::ListStore<Gtk::FileFilter>>  _openNorFileFilters = nullptr;

    /* Serial Stuff */
    CodeDatabase _codeDatabase;
    Glib::Dispatcher _communicationDispatcher;
    Glib::RefPtr<Gtk::TextBuffer> _logBuffer = nullptr;
    Glib::RefPtr<Gtk::Button> _connectButton = nullptr;
    Glib::RefPtr<Gtk::Button> _readCodesButton = nullptr;
    Glib::RefPtr<Gtk::Button> _clearCodesButton = nullptr;
    Glib::RefPtr<Gtk::Button> _saveErrorLogButton = nullptr;
    Glib::RefPtr<Gtk::Button> _clearErrorLogButton = nullptr;
    Glib::RefPtr<Gtk::Button> _updateErrorCodesButton = nullptr;

    Glib::RefPtr<Gtk::Label> _dbStatusLabel = nullptr;
    std::unique_ptr<CommunicationLayer> _serialComm;

    /* NOR Flash stuff */
    NorFlashFile _norFlashFile;

    Glib::RefPtr<Gtk::Button> _openFlashFileButton = nullptr;

    Glib::RefPtr<Gtk::TextBuffer> _norMd5 = nullptr;
    Glib::RefPtr<Gtk::TextBuffer> _norBoardId = nullptr;
    Glib::RefPtr<Gtk::ComboBox> _norModelCombo = nullptr;
    Glib::RefPtr<Gtk::TextBuffer> _norSku = nullptr;
    Glib::RefPtr<Gtk::TextBuffer> _norSerial = nullptr;
    Glib::RefPtr<Gtk::TextBuffer> _norBoardSerial = nullptr;
    Glib::RefPtr<Gtk::ComboBox> _norRegionCombo = nullptr;
    Glib::RefPtr<Gtk::TextBuffer> _norKibanId = nullptr;
    Glib::RefPtr<Gtk::TextBuffer> _norCurrentFirmware = nullptr;
    Glib::RefPtr<Gtk::TextBuffer> _norFirmwareLimits = nullptr;
    Glib::RefPtr<Gtk::TextBuffer> _norLanMac = nullptr;
    Glib::RefPtr<Gtk::TextBuffer> _norWifiMac = nullptr;

    Glib::RefPtr<Gtk::Label> _norStatusLabel = nullptr;

    Glib::RefPtr<Gtk::ListStore> _norModelItems = nullptr;
    Glib::RefPtr<Gtk::ListStore> _norRegionItems = nullptr;

protected:
    void on_activate() override;

    /* Serial stuff */
    void on_communication_event() const;
    void on_connect_clicked() const;
    void on_read_codes_clicked() const;
    void on_clear_codes_clicked() const;
    void on_save_error_log_clicked();
    void on_clear_error_log_clicked();
    void on_update_error_codes_clicked() const;
    void on_save_error_log_fileSelected(const Glib::RefPtr<Gio::AsyncResult>&) const;

    /* NOR Flash stuff */
    void on_openFlashFile_clicked();
    void on_openFlashFile_fileSelected(const Glib::RefPtr<Gio::AsyncResult>&);


private:
    void toggleUartButtons(bool enabled) const;

    void createComboBoxModel();

    class ModelComboColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:

        ModelComboColumns()
        {
            add(m_col_id);
            add(m_col_name);
        }

        Gtk::TreeModelColumn<int> m_col_id;
        Gtk::TreeModelColumn<Glib::ustring> m_col_name;
    };

    class RegionComboColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        RegionComboColumns()
        {
            add(m_col_id);
            add(m_col_region);
        }

        Gtk::TreeModelColumn<int> m_col_id;
        Gtk::TreeModelColumn<Glib::ustring> m_col_region;
    };

    ModelComboColumns _modelComboColumns;
    RegionComboColumns _regionComboColumns;

};
