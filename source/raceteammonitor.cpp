#include "raceteammonitor.h"

RaceTeamMonitor::RaceTeamMonitor(QWidget* parent)
{
    _ui.setupUi(this);

    this->setWindowTitle("RaceTeamMonitor");

    // fixed name of configuration file
    const std::string cfg_file = "konfiguration.toml";
    _event = parseEvent(cfg_file);

    // create directory if not existent
    if (!std::filesystem::exists(_event.data_path))
        std::filesystem::create_directory(_event.data_path);

    if (_event.drivers.empty())
    {
        QMessageBox msgBox;
        msgBox.setText(QStringLiteral("Das Event wurde noch nicht vollständig konfiguriert. ")
                      +QStringLiteral("Es müssen zunächst alle notwendigen Einstellungen erfolgen bevor der RaceTeamMonitor genutzt werden kann. ")
                      +QStringLiteral("Dazu die Datei \"konfiguration.toml\" im Installationsverzeichnis mit einem Texteditor öffnen und die Konfiguration vornehmen. ")
                      +QStringLiteral("Danach muss der RaceTeamMonitor neugestartet werden."));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    }

    // create 0000.laps if not existent and init plots
    if(_event.drivers.size() > 0)
        initEvent();

    // in the beginning all durations are predicted
    _current_idx = 0; // index of first predicted lap

    _ui.clock_label->setText(QTime::currentTime().toString(QString("hh:mm")));
    _ui.change_label->setAlignment(Qt::AlignCenter);

    // time will trigger timeout() each second
    QTimer* timer = new QTimer(this);
    timer->start(1000);
    connect(timer, SIGNAL(timeout()), this, SLOT(clocksTicker()));

    // create initial table based on latest available *.laps file in folder
    createTable(readLatestInfos(QString::fromStdString(_event.data_path)));

    // based on the created table, the tableTicker can run
    connect(timer, SIGNAL(timeout()), this, SLOT(tableTicker()));

    // in case of changing racestart, the table will be recreated
    QObject::connect(_ui.racestart_datetimeedit, &QDateTimeEdit::dateTimeChanged, this,
        [this]() {
            _event.race_begin = _ui.racestart_datetimeedit->dateTime();
            createTable(_current_infos);
        });

    // connect change button with change dialog
    connect(_ui.change_min_pushbutton, SIGNAL(clicked()), this, SLOT(clickedChange()));
    connect(_ui.change_5min_pushbutton, SIGNAL(clicked()), this, SLOT(clickedChange()));
    connect(_ui.change_2min_pushbutton, SIGNAL(clicked()), this, SLOT(clickedChange()));
    connect(_ui.change_1min_pushbutton, SIGNAL(clicked()), this, SLOT(clickedChange()));
    connect(_ui.change_30sek_pushbutton, SIGNAL(clicked()), this, SLOT(clickedChange()));
    connect(_ui.change_10sek_pushbutton, SIGNAL(clicked()), this, SLOT(clickedChange()));

    connect(new QShortcut(QKeySequence(Qt::ALT + Qt::Key_0), this), &QShortcut::activated,
        [this]() {_ui.change_10sek_pushbutton->clicked(); });
    connect(new QShortcut(QKeySequence(Qt::ALT + Qt::Key_3), this), &QShortcut::activated,
        [this]() {_ui.change_30sek_pushbutton->clicked(); });
    connect(new QShortcut(QKeySequence(Qt::ALT + Qt::Key_1), this), &QShortcut::activated,
        [this]() {_ui.change_1min_pushbutton->clicked(); });
    connect(new QShortcut(QKeySequence(Qt::ALT + Qt::Key_2), this), &QShortcut::activated,
        [this]() {_ui.change_2min_pushbutton->clicked(); });
    connect(new QShortcut(QKeySequence(Qt::ALT + Qt::Key_5), this), &QShortcut::activated,
        [this]() {_ui.change_5min_pushbutton->clicked(); });

    // let tickers tick
    connect(timer, SIGNAL(timeout()), this, SLOT(labelTicker()));
    connect(timer, SIGNAL(timeout()), this, SLOT(statsTicker()));

    // show minimal information
    QObject::connect(_ui.info_event, &QAction::triggered, this,
        [&]() {
            
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("About");
            msgBox.setTextFormat(Qt::RichText);
            std::stringstream ss;
            ss << "RaceTeamMonitor v0.2 Juli 2022 | ";
            ss << "<a href='https://github.com/aiddink/RaceTeamMonitor'>https://github.com/aiddink/RaceTeamMonitor</a>";
            msgBox.setText(QString::fromStdString(ss.str()));
            msgBox.exec();

        });

    // if user wants to insert or edit the driver of a lap
    QObject::connect(_ui.change_order_action, &QAction::triggered, this,
        [&]() {

            QDialog* dialog = new QDialog();
            Ui::OrderChange* change = new Ui::OrderChange; // ui_tmp(changetime);
            change->setupUi(dialog);

            // update dialog elements
            change->new_lap_spinbox->setValue(_current_idx+1); // +1 because lap counting starts with 1
            for (auto& driver : _event.drivers)
                change->new_driver_combobox->addItem(QString::fromStdString(driver.name));

            dialog->show();

            // if user really wants to make changes
            if (dialog->exec() == QDialog::Accepted)
            {
                std::string new_driver = change->new_driver_combobox->currentText().toStdString();
                int selected_idx = change->new_lap_spinbox->value() - 1; // +1 because lap counting starts with 1

                // get any lap of selected driver with predicted time
                auto lapinfo_copy = *std::find_if(_current_infos.begin(), _current_infos.end(), [new_driver](LapInfo& tmp) {
                    return tmp.name == new_driver && tmp.type == "P"; // we want the predicted and not any measured time
                });

                lapinfo_copy.type = "M";
                if (selected_idx >= _current_idx)
                    lapinfo_copy.type = "P";

                if (change->replace_radiobutton->isChecked())
                    _current_infos.at(selected_idx) = lapinfo_copy;
                else if (change->insert_radiobutton->isChecked())
                    _current_infos.insert(_current_infos.begin() + selected_idx, lapinfo_copy);

                // update file and view
                _current_infos = writeLatestInfos(QString::fromStdString(_event.data_path), _current_infos);
                createTable(_current_infos);
            }
        });

    // if user wants to insert or edit the driver of a lap
    QObject::connect(_ui.break_event_action, &QAction::triggered, this,
        [&]() {

            QDialog* dialog = new QDialog();
            Ui::OrderChange* change = new Ui::OrderChange; // ui_tmp(changetime);
            change->setupUi(dialog);

            // update dialog elements
            change->new_lap_spinbox->setValue(_current_idx + 1); // +1 because lap counting starts with 1
            change->new_driver_combobox->addItem(QString::fromStdString("PAUSE"));
            change->insert_radiobutton->setChecked(true);
            change->replace_radiobutton->setChecked(false);
            change->replace_radiobutton->hide();
            change->label->setText("");
            dialog->show();

            // if user really wants to make changes
            if (dialog->exec() == QDialog::Accepted)
            {
                int selected_idx = change->new_lap_spinbox->value() - 1; // +1 because lap counting starts with 1

                LapInfo break_info;
                break_info.name = "PAUSE";
                break_info.duration = 0.0;
                break_info.type = "P";

                _current_infos.insert(_current_infos.begin() + selected_idx, break_info);

                // update file and view
                _current_infos = writeLatestInfos(QString::fromStdString(_event.data_path), _current_infos);
                createTable(_current_infos);
            }
        });

    // reload infos from most recent file within data-dir
    QObject::connect(_ui.reload_laps_action, &QAction::triggered, this,
        [&]() {
            _current_infos = readLatestInfos(QString::fromStdString(_event.data_path));
            createTable(_current_infos);
        });

    // delete data-dir and reinit event
    QObject::connect(_ui.delete_laps_action, &QAction::triggered, this,
        [this]() {

            QMessageBox msgBox;
            msgBox.setText(QStringLiteral("Alle bisherigen Rundenzeiten werden unwiderruflich gelöscht."));
            msgBox.setInformativeText("Sind Sie sicher?");
            msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
            msgBox.setDefaultButton(QMessageBox::Cancel);
            int ret = msgBox.exec();

            if(ret == QMessageBox::Ok)
            {
                std::filesystem::remove_all(_event.data_path);
                std::filesystem::create_directory(_event.data_path);
                initEvent();
                _current_infos = readLatestInfos(QString::fromStdString(_event.data_path));
                createTable(_current_infos);
            }

        });

    // delete last change --> delete last 000X.laps file
    QObject::connect(_ui.delete_change_action, &QAction::triggered, this,
        [&]() {

            QDir dir(QString::fromStdString(_event.data_path));
            dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
            dir.setSorting(QDir::Name | QDir::Reversed);
            dir.setNameFilters(QStringList() << "*.laps");

            if (!dir.entryInfoList().isEmpty())
            {
                // get latest file (e.g. 0003.laps)
                QFileInfo fileInfo = dir.entryInfoList().at(0);

                // don't delete initial file
                std::string check_not_0000 = fileInfo.fileName().toStdString();
                if(check_not_0000 != "0000.laps")
                    std::remove(fileInfo.absoluteFilePath().toStdString().c_str());

                _current_infos = readLatestInfos(QString::fromStdString(_event.data_path));
                createTable(_current_infos);
            }
        });

    /* Event-CLASS UNDER DEVELOPMENT - NOT IN USE
    // if user wants to edit or create an event
    QObject::connect(_ui.create_event_action, &QAction::triggered, this,
        [this, cfg_file]() {

            do
            {
                Event* dialog = new Event(this, _event);
                dialog->show();

                // if user really wants to make changes
                if (dialog->exec() == QDialog::Accepted)
                {
                   dialog->writeConfigToFile(cfg_file);
                   _event = parseEvent(cfg_file);
                   if (_event.drivers.size() > 0)
                   {
                       initEvent();
                       _current_infos = readLatestInfos(QString::fromStdString(_event.data_path));
                       createTable(_current_infos);
                       break;
                   }
                }
                else
                    break;
            }
            while (true);
        });
        */
}
// ...........................................................................
EventInfo RaceTeamMonitor::parseEvent(std::string cfg_file)
// ...........................................................................
{
    EventInfo tmp_event;

    std::shared_ptr<cpptoml::table> config;
    try
    {
        config = cpptoml::parse_file(cfg_file);
    }
    catch (std::exception& e)
    {
        QMessageBox(QMessageBox::Icon::Critical, "", "Datei \"konfiguration.toml\" konnte nicht erfolgreich gelesen werden. Syntax fehlerhaft.").exec();
    }

    std::string race_begin_time = *config->get_qualified_as<std::string>("rennbeginn_uhrzeit");        // rennbeginn_uhrzeit = "14:00"
    double race_duration = *config->get_qualified_as<double>("renndauer_stunden");                   // renndauer_stunden = 24.0
    std::vector<std::string> drivers = *config->get_qualified_array_of<std::string>("fahrer_name");  // fahrer_name = ["Chris", "Andreas"]
    std::vector<double> times = *config->get_qualified_array_of<double>("erwartete_rundenzeit");       // erwartete_rundenzeit = [51.0, 51.0]
    std::vector<double> change_intervals = *config->get_qualified_array_of<double>("wechsel_intervall"); // wechsel_intervall = [1,1]
    std::vector<std::string> glympse_ids = *config->get_qualified_array_of<std::string>("glympse_karten_id");   // fahrer_name = ["Chris", "Andreas"]
    double lap_length = *config->get_qualified_as<double>("runden_laenge_km");                        // runden_laenge_km = 25.8
    double formation_lap_length = *config->get_qualified_as<double>("formations_runden_laenge_km");

    // use current day and only update hours and minutes
    auto date_time = QDateTime::currentDateTime();
    date_time.setTime(QTime::fromString(QString::fromStdString(race_begin_time), "hh:mm"));

    for (int i = 0; i < drivers.size(); ++i)
    {
        tmp_event.drivers.push_back({ drivers.at(i) });

        auto& driver = tmp_event.drivers.back();

        if (times.size() > i)
            driver.pre_lap_duration = times.at(i);

        if (glympse_ids.size() > i)
            driver.glympse_id = glympse_ids.at(i);

        if (change_intervals.size() > i)
            driver.change_interval = change_intervals.at(i);
    }

    // check if arrays are of equal length
    if (drivers.size() != times.size() || times.size() != glympse_ids.size() || times.size() != change_intervals.size())
    {
        std::stringstream ss;
        ss << "Ungleiche Anzahl der Elemente in den Feldern \"fahrer_name\", \"erwartete_rundenzeit\", \"wechsel_intervall\" sowie \"glympse_karten_id\" ";
        ss << "in der Konfigurationsdatei. Alle Felder sollten die gleiche Anzahl an Elementen (" << drivers.size() << ") haben.";
        QMessageBox(QMessageBox::Icon::Warning, "", QString::fromStdString(ss.str())).exec();
        return EventInfo();
    }

    tmp_event.name = "event"; // could be extended in future to store multiple events within config
    tmp_event.data_path = "./data/"; // might be changed in future as well
    tmp_event.race_begin = date_time;
    tmp_event.race_duration_hours = race_duration;
    tmp_event.lap_length = lap_length;
    tmp_event.formation_lap_diff = lap_length - formation_lap_length;

    return tmp_event;
}
// ...........................................................................
void RaceTeamMonitor::initEvent()
// ...........................................................................
{
    QDir dir(QString::fromStdString(_event.data_path));
    dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name | QDir::Reversed);
    dir.setNameFilters(QStringList() << "*.laps");

    // create initial laps-file if direcoty is empty
    // determine the number of possible laps
    if (dir.entryInfoList().isEmpty())
    {
        std::stringstream new_path;
        new_path << dir.absolutePath().toStdString() << "/" << std::setw(4) << std::setfill('0') << std::right << "0" << ".laps";

        std::ofstream out(new_path.str().c_str());

        if (!out.is_open())
            QMessageBox(QMessageBox::Icon::Warning, "", "Fehler beim Schreiben der laps-Datei").exec();

        // break loops if event duration is reached, e.g. 24 hours
        double duration = 0.0;
        do
        {
            for (auto& driver : _event.drivers)
            {
                for (int interval = 0; interval < driver.change_interval; interval++)
                {
                    // adjust prediction due to shorter formation lap
                    double diff_prediction = 0.0;
                    if (duration == 0.0 && _event.formation_lap_diff != 0)
                    {
                        double relation = _event.formation_lap_diff / _event.lap_length;
                        diff_prediction = relation * driver.pre_lap_duration;
                    }

                    out << driver.name << " " << std::fixed << std::setprecision(4) << driver.pre_lap_duration - diff_prediction << " " << "P" << std::endl;
                    duration += (driver.pre_lap_duration - diff_prediction);

                    if (duration >= (_event.race_duration_hours * 60.0))
                        break;
                }

                if (duration >= (_event.race_duration_hours * 60.0))
                    break;
            }
        } while (duration < (_event.race_duration_hours * 60.0));

        out.close();
    }

    // set valid race begin time
    _ui.racestart_datetimeedit->setDateTime(_event.race_begin);

    // prepare stats table at the bottom of the gui once with correct labels using names of drivers
    const int n_stats = 4; // because we got 4 rows defined within Qt Designer
    _ui.stats_tablewidget->setRowCount(n_stats);
    _ui.stats_tablewidget->setColumnCount(_event.drivers.size());

    QStringList labels;
    for (int c = 0; c < _event.drivers.size(); c++)
    {
        labels << QString::fromStdString(_event.drivers.at(c).name);
        for (int r = 0; r < n_stats; r++)
            _ui.stats_tablewidget->setItem(r, c, new QTableWidgetItem(""));

        // only add tab if a glympse-id is defined for this driver
        // and only add new tabs if not already existent from previous initEvent()
        if (!_event.drivers.at(c).glympse_id.empty() && _ui.main_tabwidget->count() <= (c + 1))
        {
            QWebEngineView* view = new QWebEngineView(this);
            view->load(QUrl("http://glympse.com/" + QString::fromStdString(_event.drivers.at(c).glympse_id)));
            view->show();
            _ui.main_tabwidget->addTab(view, QString::fromStdString("Karte (" + _event.drivers.at(c).name + " | " + _event.drivers.at(c).glympse_id + ")"));
        }
    }
    _ui.stats_tablewidget->setHorizontalHeaderLabels(labels);
}
// ...........................................................................
std::vector<LapInfo> RaceTeamMonitor::readLatestInfos(QString path)
// ...........................................................................
{
    std::vector<LapInfo> infos;

    QDir dir(path);
    dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name | QDir::Reversed);
    dir.setNameFilters(QStringList() << "*.laps");

    // get latest file at position "0" (e.g. 0003.laps)
    if (dir.entryInfoList().size() > 0)
    {
        QFileInfo fileInfo = dir.entryInfoList().at(0);

        std::string line;
        std::ifstream inStream;
        inStream.open(fileInfo.absoluteFilePath().toStdString().c_str(), std::ios::in);
        while (std::getline(inStream, line))
        {
            LapInfo tmp;
            std::istringstream info_line(line);
            info_line >> tmp.name >> tmp.duration >> tmp.type;
            infos.push_back(tmp);
        }
    }
    return infos;
}
// ...........................................................................
void RaceTeamMonitor::createTable(std::vector<LapInfo> infos)
// ...........................................................................
{
    _stats_mapper.clear();
    _ui.plot_widget->clearGraphs();
    _ui.race_tablewidget->setRowCount(0);

    int current_row;
    QDateTime lastStartTime = _ui.racestart_datetimeedit->dateTime();
    QDateTime lastEndTime = lastStartTime;

    bool predicted_pause = false;
    bool first_time_P = false;
    std::map<std::string, int> driver_laps;
    for (uint i = 0; i < infos.size(); i++)
    {
        // determine number of measured and predicted maps for graph
        driver_laps[infos.at(i).name]++;

        _ui.race_tablewidget->setRowCount(_ui.race_tablewidget->rowCount() + 1);
        current_row = _ui.race_tablewidget->rowCount() - 1;

        // name of rider in first column
        QTableWidgetItem* item1 = new QTableWidgetItem(QString::fromStdString(infos.at(i).name));
        item1->setTextAlignment(Qt::AlignCenter);
        _ui.race_tablewidget->setItem(current_row, 0, item1);

        // beginning of lap in second column
        infos.at(i).start = lastStartTime;
        QTableWidgetItem* item2 = new QTableWidgetItem(lastStartTime.toString(QString("hh:mm:ss")));
        if (infos.at(i).type == "P")
            item2->setTextColor(Qt::red);

        // end of lap (predicted or measured) in third column
        lastEndTime = lastEndTime.addSecs(infos.at(i).duration * 60.0);
        infos.at(i).end = lastEndTime;
        QTableWidgetItem* item3 = new QTableWidgetItem(lastEndTime.toString(QString("hh:mm:ss")));

        // if predicted, start and end time in red text color
        if (infos.at(i).type == "P")
            item3->setTextColor(Qt::red);

        // indicates a PAUSE
        if (infos.at(i).duration == 0.0 && infos.at(i).type == "P")
        {
            predicted_pause = true;
            _ui.race_tablewidget->setSpan(current_row, 0, 1, 4);
        }
        // don't show any further start and end times after PAUSE because
        // there is no sensible predication of a PAUSE end possible
        else if(predicted_pause == false)
        {
            _ui.race_tablewidget->setItem(current_row, 1, item2);
            _ui.race_tablewidget->setItem(current_row, 2, item3);
        }

        // in case of current lap, edit text of label for next change
        if (infos.at(i).type == "P" && first_time_P == false)
        {
            // if current driver drives multiple laps in a row, the next driver is of interest for the label and not the current
            if ((i+1 < infos.size() && infos.at(i).name != infos.at(i + 1).name ) || i == (infos.size()-1))
            {
                first_time_P = true;
                _current_idx = current_row;
                _ui.change_groupbox->setTitle(QString("Wechsel von " + QString::fromStdString(infos.at(i).name) + " nach " + QString::fromStdString(infos.at(i + 1).name)) + " erfolgte vor ca.");
            }
         }

        lastStartTime = lastEndTime;
    }

    // save current info vector for access in other methods
    _current_infos = infos;

    // init table widget with start-information
    _ui.race_tablewidget->setSelectionMode(QAbstractItemView::SelectionMode::NoSelection);
    _ui.race_tablewidget->resizeColumnsToContents();
    _ui.race_tablewidget->setCurrentCell(_current_idx, 0);

    // prepare plot using max number of laps plus one for x-axis
    auto max_element = std::max_element(std::begin(driver_laps), std::end(driver_laps), [](auto& p1, auto& p2) {return p1.second < p2.second; });
    _ui.plot_widget->xAxis->setRange(0, max_element->second+1);

    const double offset = 5.0;
    double min = 1e6, max = -1e6;
    int driver_cnt = 0;
    for (auto& driver : _event.drivers)
    {
        // map drivers name to specific column and graph-idx
        _stats_mapper[driver.name] = driver_cnt;

        _ui.plot_widget->addGraph(_ui.plot_widget->xAxis, _ui.plot_widget->yAxis);
        _ui.plot_widget->graph()->setPen(QPen((Qt::GlobalColor)(Qt::GlobalColor::red + driver_cnt++)));
        _ui.plot_widget->graph()->setLineStyle(QCPGraph::lsLine);
        _ui.plot_widget->graph()->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5));
        _ui.plot_widget->graph()->setName(QString::fromStdString(driver.name));

        double tmp = driver.pre_lap_duration;
        if (tmp < min) min = tmp;
        if (tmp > max) max = tmp;
    }

    _ui.plot_widget->legend->setVisible(true);
    _ui.plot_widget->yAxis->setRange(min- offset, max + offset);
    _ui.plot_widget->xAxis->setLabel("Anzahl Runden [#]");
    _ui.plot_widget->yAxis->setLabel("Rundenzeit [min]");
    _ui.plot_widget->yAxis2->setLabel("Geschwindigkeit Ø");
    _ui.plot_widget->replot();
}
// ...........................................................................
void RaceTeamMonitor::labelTicker()
// ...........................................................................
{
    if (_current_infos.size() > 0)
    {
        // determine measured duration
        double measured_duration = 0.0;
        for (auto& tmp_info : _current_infos)
            if (tmp_info.type == "M")
                measured_duration += tmp_info.duration;

        auto race_time = timeDiff(QDateTime::currentDateTime(), _event.race_begin);
        double race_duration = race_time.hour() * 60.0 + race_time.minute();

        LapInfo info = _current_infos.at(_current_idx);
        if (QDateTime::currentDateTime() > _ui.racestart_datetimeedit->dateTime())
        {
            if (measured_duration > race_duration)
            {
                _ui.change_label->setText(QStringLiteral("Inkonsistent! Fahrzeit > Eventdauer"));
            }
            else if (QDateTime::currentDateTime() < info.end)
            {
                QTime diff_time = timeDiff(info.end, QDateTime::currentDateTime());
                double minutes = diff_time.hour() * 60.0 + diff_time.minute();
                std::stringstream text;
                text << _current_infos.at(_current_idx + 1).name << " ist dran in " << minutes << "min und " << diff_time.second() << "sek";

                // dependent on the remaining time, the illustration will be different
                _ui.change_label->setText(QString::fromStdString(text.str()));
                if (minutes >= 10)
                {
                    _ui.change_label->setStyleSheet("QLabel { color : black; }");
                }
                else if (minutes < 10 && minutes >= 5)
                {
                    _ui.change_label->setStyleSheet("QLabel { color : red; }");
                }
                else if (minutes < 5)
                {
                    if (_ui.change_label->styleSheet().toStdString().find("black") != std::string::npos)
                        _ui.change_label->setStyleSheet("QLabel { color : red; }");
                    else
                        _ui.change_label->setStyleSheet("QLabel { color : black; }");
                }

                _ui.changezone_groupbox->setStyleSheet("QGroupBox { color : black; }");
            }
            else
            {
                QTime diff_time = timeDiff(QDateTime::currentDateTime(), info.end);

                std::stringstream text;
                if (info.duration == 0.0) // indicates a PAUSE
                {
                    text << diff_time.hour() << "h " << diff_time.minute() << "min";
                    _ui.change_label->setText(QStringLiteral("Pause seit ") + QString::fromStdString(text.str()));
                    _ui.change_label->setStyleSheet("QLabel { color : black; }");
                    _ui.changezone_groupbox->setStyleSheet("QGroupBox { color : black; }");
                }
                else
                {
                    double minutes = diff_time.hour() * 60.0 + diff_time.minute();
                    text << minutes << "min und " << diff_time.second() << "sek";
                    _ui.change_label->setText(QStringLiteral("Überfällig seit ") + QString::fromStdString(text.str()));

                    if (_ui.change_label->styleSheet().toStdString().find("background-color : red") != std::string::npos)
                    {
                        _ui.change_label->setStyleSheet("QLabel { background-color : black; color : red; }");
                        _ui.changezone_groupbox->setStyleSheet("QGroupBox { background-color : black; color : red; }");
                    }
                    else
                    {
                        _ui.change_label->setStyleSheet("QLabel { background-color : red; color : black; }");
                        _ui.changezone_groupbox->setStyleSheet("QGroupBox { background-color : red; color : black; }");
                    }
                }
            }
        }
        else // if race not started yet
        {
            auto diff = timeDiff(_ui.racestart_datetimeedit->dateTime(), QDateTime::currentDateTime());
            std::stringstream text;
            if (diff.hour() > 0)
                text << "Rennen startet in " << diff.hour() << "h " << diff.minute() << "min";
            else
                text << "Rennen startet in " << diff.minute() << "min " << diff.second() << "sek";

            _ui.change_label->setText(QString::fromStdString(text.str()));
            _ui.change_label->setStyleSheet("QLabel { color : black; }");
            _ui.changezone_groupbox->setStyleSheet("QGroupBox { color : black; }");
        }
    }
    else
    {
        _ui.change_label->setText("Kein Event initiiert");
        _ui.change_label->setStyleSheet("QLabel { color : black; }");
        _ui.changezone_groupbox->setStyleSheet("QGroupBox { color : black; }");
    }
}
// ...........................................................................
void RaceTeamMonitor::tableTicker()
// ...........................................................................
{
    // additional laps are used if a driver drives multiple laps in a row
    int additional_laps = 0;
    for(int idx=0; idx < _current_infos.size(); idx++)
    {
        auto& info = _current_infos.at(idx);

        if (idx + 1 < _current_infos.size())
        {
            auto& next_info = _current_infos.at(idx+1);
            if (info.name == next_info.name)
            {
                additional_laps++;
                continue;
            }
        }

        QDateTime ref_start = _current_infos.at(idx - additional_laps).start;

        int column = 3;
        QTableWidgetItem *item_c3, *item_c4;
        QTime current_drive_time = timeDiff(QDateTime::currentDateTime(), ref_start);
        item_c4 = new QTableWidgetItem(current_drive_time.toString(QString("hh:mm:ss")));

        // show ticking drive time of current lap
        if (idx == _current_idx && QDateTime::currentDateTime() > _ui.racestart_datetimeedit->dateTime()
            && QDateTime::currentDateTime() > ref_start )
        {
            if (additional_laps > 0)
                _ui.race_tablewidget->setSpan(idx - additional_laps, 4, additional_laps + 1, 1);

            _ui.race_tablewidget->setItem(idx - additional_laps, 4, item_c4);
        }

        // dont move on if a recent PAUSE is reached
        if (info.duration == 0.0 && info.type == "P")
            break;

        // if predicted and still in time
        if (QDateTime::currentDateTime() < info.end && info.type == "P")
        {
            QTime diff_time = timeDiff(info.end, QDateTime::currentDateTime());
            item_c3 = new QTableWidgetItem(diff_time.toString(QString("hh:mm:ss")));
            if(idx == _current_idx)
                item_c3->setBackgroundColor(Qt::green);
            else
                item_c3->setBackgroundColor(Qt::yellow);
        }
        // if predicted and overdue
        else if (QDateTime::currentDateTime() >= info.end && info.type == "P")
        {
            QTime diff_time = timeDiff(QDateTime::currentDateTime(), info.end);
            item_c3 = new QTableWidgetItem("-"+diff_time.toString(QString("hh:mm:ss")));
            item_c3->setBackgroundColor(Qt::red);
        }
        // if measured and finished
        else if (info.type == "M")
        {
            QTime diff_time = timeDiff(info.end, info.start);
            item_c3 = new QTableWidgetItem(diff_time.toString(QString("hh:mm:ss")));
            column = 4; // put finished lap time in different column
        }
        else
            item_c3 = new QTableWidgetItem("");

        if(additional_laps > 0)
            _ui.race_tablewidget->setSpan(idx - additional_laps, column, additional_laps + 1, 1);

        _ui.race_tablewidget->setItem(idx - additional_laps, column, item_c3);

        additional_laps = 0;
    }
    
    _ui.race_tablewidget->resizeColumnsToContents();
    _ui.race_tablewidget->setCurrentCell(_current_idx, 0);
}
// ...........................................................................
void RaceTeamMonitor::clocksTicker()
// ...........................................................................
{
    if (QDateTime::currentDateTime() > _ui.racestart_datetimeedit->dateTime())
    {
        QTime diff_time = timeDiff(QDateTime::currentDateTime(), _ui.racestart_datetimeedit->dateTime().addDays(1));
        _ui.race_label->setText(diff_time.toString(QString("hh:mm")));
    }
    else // not started yet
        _ui.race_label->setText("--:--");

    _ui.clock_label->setText(QTime::currentTime().toString(QString("hh:mm")));
}
// ...........................................................................
std::vector<LapInfo> RaceTeamMonitor::writeLatestInfos(QString path, std::vector<LapInfo> infos)
// ...........................................................................
{
    QDir dir(path);
    dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name | QDir::Reversed);
    dir.setNameFilters(QStringList() << "*.laps");

    // get latest file (e.g. 0003.laps)
    QFileInfo fileInfo = dir.entryInfoList().at(0);

    // calculate new_idx (e.g. 0004.laps)
    int new_idx = stoi(fileInfo.fileName().toStdString().substr(0, 4)) + 1;

    std::stringstream new_path;
    new_path << fileInfo.absolutePath().toStdString() << "/" << std::setw(4) << std::setfill('0') << std::right << new_idx << ".laps";

    std::ofstream out(new_path.str().c_str());

    if (!out.is_open())
        QMessageBox(QMessageBox::Icon::Warning, "", "Failed to open file for writing").exec();

    std::vector<LapInfo> written_infos;

    double duration = 0.0;
    for (auto& info : infos)
    {
        written_infos.push_back(info);
        out << info.name << " " << std::fixed << std::setprecision(4) << info.duration << " " << info.type << std::endl;
        duration += info.duration;

        if(duration >= (_event.race_duration_hours * 60.0))
            break;
    }

    out.close();

    return written_infos;
}
// ...........................................................................
void RaceTeamMonitor::statsTicker()
// ...........................................................................
{
    struct stats {
        bool formation_lap = false;
        double ridetime = 0.0; // accumulated ridetime
        int laps = 0; // number of driven laps
        QVector<double> x, y; // data points used for graph
    };

    double min_dur = 1e3, max_dur = -1e3;
    std::map<std::string, stats> riders;
    for (auto& info : _current_infos)
    {
        // ignore first formation lap and all predicted ones
        if (info.type == "M" && info.name != "PAUSE")
        {
            riders[info.name].ridetime += info.duration;
            riders[info.name].laps++;

            // add data for ploting
            riders[info.name].x.push_back(riders[info.name].laps);
            riders[info.name].y.push_back(info.duration);

            if (info.duration < min_dur)
                min_dur = info.duration;

            if (info.duration > max_dur)
                max_dur = info.duration;

            // first row of *.laps file correspondes to formation lap
            if (info.duration == _current_infos.at(0).duration)
                riders[info.name].formation_lap = true;
        }
    }

    for (auto& rider : riders)
    {
        int c = _stats_mapper[rider.first];

        _ui.plot_widget->graph(c)->setData(rider.second.x, rider.second.y);

        int n_laps = rider.second.laps;
       _ui.stats_tablewidget->item(0, c)->setText(QString::number(n_laps));

        // mean lap time in minutes
        double mean = rider.second.ridetime / rider.second.laps;
        double minutes_floor = floor(mean);
        std::stringstream mean_str;
        mean_str << minutes_floor << "m " << floor((mean - minutes_floor) * 60.0) << "s";
        _ui.stats_tablewidget->item(2, c)->setText(QString::fromStdString(mean_str.str()));

        double hours = rider.second.ridetime / 60.0;
        double sum_km = (rider.second.laps * _event.lap_length);

        // driver of formations lap drove less
        if (rider.second.formation_lap)
            sum_km -= _event.formation_lap_diff;

        double v_mean = sum_km / hours;
        double hours_floor = floor(hours);
        mean_str.str("");
        mean_str << hours_floor << "h " << floor((hours - hours_floor) * 60.0) << "m";
        _ui.stats_tablewidget->item(1, c)->setText(QString::fromStdString(mean_str.str()));

        // mean speed
        mean_str.str("");
        mean_str << std::fixed << std::setprecision(1) << v_mean << "km/h";
        _ui.stats_tablewidget->item(3, c)->setText(QString::fromStdString(mean_str.str()));
           
        c++;
    }

    const double offset = 5.0; // 5 minutes offset for visualization
    _ui.plot_widget->yAxis->setRange(min_dur - offset, max_dur + offset);

    // reset to empty fields if data-sets have been deleted
    if (riders.empty())
        for (int c = 0; c < _event.drivers.size(); c++)
            for(int r = 0; r < 4; r++)
                _ui.stats_tablewidget->item(r, c)->setText("");

    _ui.plot_widget->replot();
}
// ...........................................................................
void RaceTeamMonitor::clickedChange()
// ...........................................................................
{
    QPushButton* clicked = qobject_cast<QPushButton*>(sender());
    if (clicked && !_current_infos.empty())
    {
        QString button_text = clicked->text();

        double delay = 0.0; // change-delay in seconds
        if (button_text == "10 sek")
            delay = 10.0;
        else if (button_text == "30 sek")
            delay = 30.0;
        else if (button_text == "1 min")
            delay = 60.0;
        else if (button_text == "2 min")
            delay = 120.0;
        else if (button_text == "5 min")
            delay = 300.0;
        else if (button_text == "min")
            delay = _ui.change_min_spinbox->value() * 60.0;

        LapInfo* current_info = &_current_infos.at(_current_idx);
        QDateTime ref_start = current_info->start;

        // determine number of interval-laps driven
        int interval_laps = 0;
        for (int idx = _current_idx; idx >= 0; idx--)
        {
            if (current_info->name == _current_infos.at(idx).name)
            {
                ref_start = _current_infos.at(idx).start;
                interval_laps++;
            }
            else
                break;
        }

        // set current lap to measured status
        current_info->type = "M";

        QTime diff_time = timeDiff(QDateTime::currentDateTime().addSecs(-delay), ref_start);
        // calculate duration in minutes between start and time of change
        double duration = diff_time.hour() * 60.0 + diff_time.minute() + diff_time.second() / 60.0;

        for (int idx = _current_idx - (interval_laps - 1); idx <= _current_idx; idx++)
        {
            _current_infos.at(idx).type = "M";
            _current_infos.at(idx).duration = duration / double(interval_laps);
        }

        // calculate new prediction for this rider
        std::vector<double> durations_M;
        for (auto& info : _current_infos)
            // ignore first formation lap
            if (info.name == current_info->name && info.type == "M" && _current_infos.at(0).duration != info.duration)
                durations_M.push_back(info.duration);

        double mean;
        // in case of formation lap
        if (durations_M.size() != 0)
        {
            mean = std::accumulate(std::begin(durations_M), std::end(durations_M), 0.0) / durations_M.size();

            for (auto& info : _current_infos)
                // ignore first formation lap
                if (info.name == current_info->name && info.type == "P" && _current_infos.at(0).duration != info.duration)
                    info.duration = mean;
        }

        // returned infos might be shorter than before
        _current_infos = writeLatestInfos(QString::fromStdString(_event.data_path), _current_infos);
        createTable(_current_infos);
    }
}
// ...........................................................................
QTime RaceTeamMonitor::timeDiff(QDateTime time_1, QDateTime time_2)
// ...........................................................................
{
    // difference in seconds
    double diff = (time_1.toMSecsSinceEpoch() - time_2.toMSecsSinceEpoch()) / 1000.0;
    diff = fmod(diff, 86400.0);

    // conversion to hours, minutes and seconds
    double hours = std::abs(diff) / 3600.0;
    double hours_floor = floor(hours);
    double min_in_hours = hours - hours_floor;
    double minutes = min_in_hours * 60.0;
    double minutes_floor = floor(minutes);
    double seconds = (minutes - minutes_floor) * 60.0;

    QTime diff_time = QTime(hours_floor, minutes_floor, seconds);

    return diff_time;
}
// ...........................................................................
