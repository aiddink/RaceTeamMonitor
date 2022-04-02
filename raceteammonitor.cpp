#include "raceteammonitor.h"

RaceTeamMonitor::RaceTeamMonitor(QWidget* parent)
{
    _ui.setupUi(this);

    ///////////////////////////////////////////
    // DEFINITIONS TO BE MADE
    _path = "C:/Users/Andreas/source/repos/TrackMyLaps/infos/";
    QDateTime use_time(QDate(2017, 7, 29), QTime(12, 22, 0));
    use_time = QDateTime::currentDateTime().addSecs(-19000);
    _ui.racestart_datetimeedit->setDateTime(use_time);
    ///////////////////////////////////////////

    // in the beginning all durations are predicted
    _current_idx = 0; // index of first predicted lap

    // set "length" of lcd's
    _ui.lcd_clock->setDigitCount(8);
    _ui.lcd_race->setDigitCount(8);
    _ui.lcd_clock->display(QTime::currentTime().toString(QString("hh:mm:ss")));

    _ui.change_label->setAlignment(Qt::AlignCenter);
    _ui.stats_tablewidget->setColumnWidth(0, 79);
    _ui.stats_tablewidget->setColumnWidth(1, 79);
    _ui.stats_tablewidget->setColumnWidth(2, 79);
    _ui.stats_tablewidget->setColumnWidth(3, 79);

    // time will trigger timeout() each second
    QTimer* timer = new QTimer(this);
    timer->start(1000);
    connect(timer, SIGNAL(timeout()), this, SLOT(clocksTicker()));

    // get latest table
    std::vector<lapinfo> infos = readLatestInfos(QString::fromStdString(_path));

    // show table of latest available *.laps file in folder
    showTable(infos);

    // based on the created table, the tableTicker can run
    connect(timer, SIGNAL(timeout()), this, SLOT(tableTicker()));

    // in case of changing racestart, the table must be refreshed
    connect(_ui.racestart_datetimeedit, SIGNAL(dateTimeChanged(QDateTime)), this, SLOT(updateTable()));

    // connect change button with change dialog
    //connect(_ui.change_pushbutton, SIGNAL(clicked()), this, SLOT(openChangeDialog()));
    //connect(_ui.change_pushbutton, SIGNAL(clicked()), this, SLOT(updateStatsTable()));

    connect(timer, SIGNAL(timeout()), this, SLOT(labelTicker()));

    // do it once after program-launch
    updateStatsTable();

    // this->showMaximized();

     /*
     // generate plot features
     QCPCurve* circuit = new QCPCurve(_ui.plot_widget->xAxis, _ui.plot_widget->yAxis);
     //import coordinates of rad am ring circuit
     string line;
     ifstream inStream(rar_coords.c_str(), ios::in);
     while (getline(inStream, line, '\n'))
         circuit->addData(QString::fromStdString(line.substr(11, 9)).toDouble(), QString::fromStdString(line.substr(0, 10)).toDouble());

     circuit->setPen(QPen(Qt::blue));
     circuit->setBrush(QBrush(QColor(0, 0, 255, 20)));

     // main panel for plotting
     _ui.plot_widget->xAxis->setRange(6.916, 7.01);
     _ui.plot_widget->yAxis->setRange(50.328, 50.384);
     _ui.plot_widget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
     _ui.plot_widget->axisRect()->setupFullAxesBox();
     _ui.plot_widget->rescaleAxes();

     // prepare arrow between text and moving cyclist
     QCPItemTracer* phaseTracer = new QCPItemTracer(_ui.plot_widget);
     _itemDemoPhaseTracer = phaseTracer; // so we can access it later in the bracketDataSlot for animation
     phaseTracer->setInterpolating(true);
     phaseTracer->setStyle(QCPItemTracer::tsPlus);

     QCPItemText* phaseTracerText = new QCPItemText(_ui.plot_widget);
     _cyclist_text = phaseTracerText;
     _cyclist_text->setText("");
     phaseTracerText->position->setType(QCPItemPosition::ptAxisRectRatio);
     phaseTracerText->setPositionAlignment(Qt::AlignRight | Qt::AlignBottom);
     phaseTracerText->position->setCoords(0.88, 0.88);
     phaseTracerText->setTextAlignment(Qt::AlignLeft);
     phaseTracerText->setFont(QFont(font().family(), 15));
     phaseTracerText->setPadding(QMargins(8, 0, 0, 0));

     QCPItemCurve* phaseTracerArrow = new QCPItemCurve(_ui.plot_widget);
     phaseTracerArrow->start->setParentAnchor(phaseTracerText->left);
     phaseTracerArrow->startDir->setParentAnchor(phaseTracerArrow->start);
     phaseTracerArrow->startDir->setCoords(-40, 0); // direction 30 pixels to the left of parent anchor (tracerArrow->start)
     phaseTracerArrow->end->setParentAnchor(phaseTracer->position);
     //    phaseTracerArrow->end->setCoords(0, 0);
     phaseTracerArrow->endDir->setParentAnchor(phaseTracerArrow->end);
     //    phaseTracerArrow->endDir->setCoords(0, 0);
     phaseTracerArrow->setHead(QCPLineEnding::esNone);
     phaseTracerArrow->setTail(QCPLineEnding::esBar);
     */
}
// ...........................................................................
std::vector<lapinfo> RaceTeamMonitor::readLatestInfos(QString path)
// ...........................................................................
{
    std::vector<lapinfo> infos;

    QDir dir(path);
    dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Name | QDir::Reversed);
    dir.setNameFilters(QStringList() << "*.laps");

    // get latest file (e.g. 0003.laps)
    if (dir.entryInfoList().size() == 0)
        throw std::runtime_error("No initial laps-file found! Create 0001.txt");

    QFileInfo fileInfo = dir.entryInfoList().at(0);

    std::string line;
    std::ifstream inStream;
    inStream.open(fileInfo.absoluteFilePath().toStdString().c_str(), std::ios::in);
    while (getline(inStream, line))
    {
        lapinfo tmp;
        std::istringstream info_line(line);
        info_line >> tmp.name >> tmp.duration >> tmp.type;
        infos.push_back(tmp);
    }

    return infos;
}
// ...........................................................................
void RaceTeamMonitor::showTable(std::vector<lapinfo> infos)
// ...........................................................................
{
    _ui.race_tablewidget->setRowCount(0);

    int current_row;
    QDateTime lastStartTime = _ui.racestart_datetimeedit->dateTime();
    QDateTime lastEndTime = lastStartTime;

    bool button = false;
    for (uint i = 0; i < infos.size(); i++)
    {
        _ui.race_tablewidget->setRowCount(_ui.race_tablewidget->rowCount() + 1);
        current_row = _ui.race_tablewidget->rowCount() - 1;

        // name of rider
        QTableWidgetItem* item1 = new QTableWidgetItem(QString::fromStdString(infos.at(i).name));
        _ui.race_tablewidget->setItem(current_row, 0, item1);

        // beginning of lap
        infos.at(i).start = lastStartTime;
        QTableWidgetItem* item2 = new QTableWidgetItem(lastStartTime.toString(QString("hh:mm:ss")));
        if (infos.at(i).type == "P")
            item2->setTextColor(Qt::red);
        _ui.race_tablewidget->setItem(current_row, 1, item2);

        // end of lap (predicted or measured)
        lastEndTime = lastEndTime.addSecs(infos.at(i).duration * 60.0);
        infos.at(i).end = lastEndTime;
        QTableWidgetItem* item3 = new QTableWidgetItem(lastEndTime.toString(QString("hh:mm:ss")));
        // predicted time in red color
        if (infos.at(i).type == "P")
            item3->setTextColor(Qt::red);
        _ui.race_tablewidget->setItem(current_row, 2, item3);

        // in case of current lap, add push button for change
        if (infos.at(i).type == "P" && button == false)
        {
            button = true;
            _current_idx = current_row;
            _ui.change_groupbox->setTitle(QString("Wechsel von " + QString::fromStdString(infos.at(i).name) + " nach " + QString::fromStdString(infos.at(i + 1).name)) + " erfolgte vor ca.");
            //_ui.change_pushbutton->setText(QString("Wechsel von " + QString::fromStdString(infos.at(i).name) + " nach " + QString::fromStdString(infos.at(i + 1).name)) + " erledigt!");
        }

        lastStartTime = lastEndTime;
    }

    // save current info vector for access in other methods
    _current_infos = infos;

    // init table widget with start-information
    _ui.race_tablewidget->setSelectionMode(QAbstractItemView::SelectionMode::NoSelection);
    _ui.race_tablewidget->resizeColumnsToContents();
    _ui.race_tablewidget->setCurrentCell(_current_idx, 0);
}
/*
// ...........................................................................
void Lapcounter::cyclistTicker()
// ...........................................................................
{

    string position_log = "/home/pi/logger_macke/position/position_" + _id_assignment[_active_cyclist] + ".log";
    //import coordinates of rad am ring circuit
    string line;
    ifstream inStream(position_log.c_str(), ios::in);

    if (!inStream.is_open())
    {
        cerr << "void Lapcounter::cyclistTicker(): Failed to open file " << position_log << endl;
        _cyclist_text->setText("Keine GPS-Daten vorhanden");
    }
    else
    {
        cerr << "opened file: " << position_log << endl;
        struct gps_info {
            double lat, lon, height, speed, acc;
            string time, id;
        };

        // get lines from external position log file (sshfs mounted)
        map< string, vector<gps_info> > users_gps;
        while (getline(inStream, line, '\n'))
        {
            gps_info tmp;
            istringstream gps_line(line);
            gps_line >> tmp.time >> tmp.lat >> tmp.lon >> tmp.height >> tmp.speed >> tmp.acc >> tmp.id;
            users_gps[_id_assignment[tmp.id]].push_back(tmp);
        }

        // generate text message within plot window
        // "2017-07-30T13:04:11.000Z" format given from gps logger app
        QTime diff_time = timeDiff(QDateTime::currentDateTime(), QDateTime::fromString(QString::fromStdString(users_gps[_active_cyclist].back().time), "yyyy-MM-ddThh:mm:ss.zzzZ").addSecs(7200));

        stringstream ss;
        ss << "letzte GPS-Position vor \n" << diff_time.minute() << "min " << diff_time.second() << "sec\n";
        ss << "mit " << users_gps[_active_cyclist].back().acc << "m Genauigkeit" << endl;
        _cyclist_text->setText(QString::fromStdString(ss.str()));

        // plot different graphs for different user-id's
        int user_idx = 0;
        for (auto& user : users_gps)
        {
            if (_id_graph[user.first] == NULL)
            {
                _id_graph[user.first] = _ui.plot_widget->addGraph();
                _id_graph[user.first]->setPen(QPen(marker_color.at(user_idx)));
                _id_graph[user.first]->setLineStyle(QCPGraph::lsNone);
                _id_graph[user.first]->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 15));
            }

            // only use last data point for plotting
            _id_graph[user.first]->clearData();
            _id_graph[user.first]->addData(user.second.back().lon, user.second.back().lat);


            if (user.first == _active_cyclist)
                _itemDemoPhaseTracer->setGraph(_id_graph[user.first]);

            user_idx++;
        }
    }
    _ui.plot_widget->replot();

}
*/
// ...........................................................................
void RaceTeamMonitor::labelTicker()
// ...........................................................................
{
    lapinfo info = _current_infos.at(_current_idx);
    if (QDateTime::currentDateTime() < info.end)
    {
        QTime diff_time = timeDiff(info.end, QDateTime::currentDateTime());

        std::stringstream text;
        text << _current_infos.at(_current_idx + 1).name << " dran in " << diff_time.minute() << "min und " << diff_time.second() << "sek!";
        _ui.change_label->setText(QString::fromStdString(text.str()));
        if (diff_time.minute() >= 10)
        {
            _ui.change_label->setStyleSheet("QLabel { color : black; }");
        }
        else if (diff_time.minute() < 10 && diff_time.minute() >= 5)
        {
            _ui.change_label->setStyleSheet("QLabel { color : red; }");
        }
        else if (diff_time.minute() < 5)
        {
            if (_ui.change_label->styleSheet().toStdString().find("black") != std::string::npos)
                _ui.change_label->setStyleSheet("QLabel { color : red; }");
            else
                _ui.change_label->setStyleSheet("QLabel { color : black; }");
        }
    }
    else
    {
        _ui.change_label->setText("Wechsel! Wechsel! Wechsel!");

        if (_ui.change_label->styleSheet().toStdString().find("background-color : red") != std::string::npos)
            _ui.change_label->setStyleSheet("QLabel { background-color : black; color : red; }");
        else
            _ui.change_label->setStyleSheet("QLabel { background-color : red; color : black; }");
    }
}
// ...........................................................................
void RaceTeamMonitor::tableTicker()
// ...........................................................................
{
    int idx = 0;
    for (auto& info : _current_infos)
    {
        QTableWidgetItem* item_c3;
        if (QDateTime::currentDateTime() < info.end && info.type == "P")
        {
            QTime diff_time = timeDiff(info.end, QDateTime::currentDateTime());
            item_c3 = new QTableWidgetItem(diff_time.toString(QString("hh:mm:ss")));
            item_c3->setBackgroundColor(Qt::green);
        }
        else if (QDateTime::currentDateTime() >= info.end && info.type == "P")
        {
            QTime diff_time = timeDiff(QDateTime::currentDateTime(), info.end);
            item_c3 = new QTableWidgetItem(diff_time.toString(QString("hh:mm:ss")));
            item_c3->setBackgroundColor(Qt::red);
        }
        else if (info.type == "M")
        {
            QTime diff_time = timeDiff(info.end, info.start);
            item_c3 = new QTableWidgetItem(diff_time.toString(QString("hh:mm:ss")));
        }
        else
            item_c3 = new QTableWidgetItem("");

        _ui.race_tablewidget->setItem(idx, 3, item_c3);

        idx++;
    }

    _ui.race_tablewidget->resizeColumnsToContents();
    _ui.race_tablewidget->setCurrentCell(_current_idx, 0);
}
// ...........................................................................
void RaceTeamMonitor::clocksTicker()
// ...........................................................................
{
    QTime diff_time = timeDiff(QDateTime::currentDateTime(), _ui.racestart_datetimeedit->dateTime().addDays(1));
    _ui.lcd_race->display(diff_time.toString(QString("hh:mm:ss")));
    _ui.lcd_clock->display(QTime::currentTime().toString(QString("hh:mm:ss")));
}
// ...........................................................................
void RaceTeamMonitor::writeLatestInfos(QString path, std::vector<lapinfo> infos)
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
    new_path << fileInfo.absolutePath().toStdString() << "/" << std::setw(4) << std::setfill('0') << right << new_idx << ".laps";

    std::ofstream out(new_path.str().c_str());

    if (!out.is_open())
        QMessageBox(QMessageBox::Icon::Warning, "", "Failed to open file for writing").exec();
    // throw runtime_error("void Lapcounter::writeLatestInfos(QString path, vector<lapinfo> infos): Failed to open file for writing: " + new_path.str());

    for (auto& info : infos)
        out << info.name << " " << fixed << std::setprecision(4) << info.duration << " " << info.type << endl;

    out.close();
}
// ...........................................................................
void RaceTeamMonitor::updateTable()
// ...........................................................................
{
    showTable(_current_infos);
}
// ...........................................................................
void RaceTeamMonitor::updateStatsTable()
// ...........................................................................
{
    struct stats {
        bool formation_lap = false;
        double ridetime; // accumulated ridetime
        int laps; // number of laps
        std::vector<double> lap_durations;
    };

    std::map<std::string, stats> riders;
    for (auto& info : _current_infos)
    {
        // ignore first formation lap and all predicted ones
        if (info.type == "M")
        {
            if (_current_infos.at(0).duration != info.duration)
            {
                riders[info.name].ridetime += info.duration;
                riders[info.name].laps++;
            }
            else
                riders[info.name].formation_lap = true;

            riders[info.name].lap_durations.push_back(info.duration);
        }
    }

    for (auto& rider : riders)
    {
        for (int i = 0; i < _ui.stats_tablewidget->columnCount(); i++)
        {
            if (_ui.stats_tablewidget->horizontalHeaderItem(i)->text().toStdString() == rider.first)
            {
                int n_laps = rider.second.laps;

                // in case of formation laps, we need to increase the number of laps
                if (rider.second.formation_lap)
                    n_laps++;

                _ui.stats_tablewidget->item(0, i)->setText(QString::number(n_laps));

                // mean lap time in minutes
                double mean = rider.second.ridetime / rider.second.laps;
                double minutes_floor = floor(mean);
                std::stringstream mean_str;
                mean_str << minutes_floor << "m " << floor((mean - minutes_floor) * 60.0) << "s";
                _ui.stats_tablewidget->item(2, i)->setText(QString::fromStdString(mean_str.str()));

                double hours = rider.second.ridetime / 60.0;
                double v_mean = (rider.second.laps * 26.0) / hours; // one lap is about 26km
                if (rider.second.formation_lap)
                    hours += (_current_infos.at(0).duration) / 60.0;

                double hours_floor = floor(hours);
                mean_str.str("");
                mean_str << hours_floor << "h " << floor((hours - hours_floor) * 60.0) << "m";
                _ui.stats_tablewidget->item(1, i)->setText(QString::fromStdString(mean_str.str()));

                // mean speed
                mean_str.str("");
                mean_str << fixed << std::setprecision(1) << v_mean << "km/h";
                _ui.stats_tablewidget->item(3, i)->setText(QString::fromStdString(mean_str.str()));

            }
        }
    }
}

// ...........................................................................
void RaceTeamMonitor::openChangeDialog()
// ...........................................................................
{
    /*
    QDialog* changetime = new QDialog();

    Ui::Change* ui_tmp = new Ui::Change; // ui_tmp(changetime);
    ui_tmp->setupUi(changetime);

    changetime->show();

    // if case of successful change
    if (changetime->exec() == QDialog::Accepted)
    {

        double delay = 0.0; // change-delay in seconds
        if (ui_tmp->secs10_radiobutton->isChecked())
            delay = 10.0;
        else if (ui_tmp->secs30_radiobutton->isChecked())
            delay = 30.0;
        else if (ui_tmp->min1_radiobutton->isChecked())
            delay = 60.0;
        else if (ui_tmp->min2_radiobutton->isChecked())
            delay = 120.0;
        else if (ui_tmp->min5_radiobutton->isChecked())
            delay = 300.0;

        // set current lap to measured status
        lapinfo* current_info = &_current_infos.at(_current_idx);
        current_info->type = "M";

        QTime diff_time = timeDiff(QDateTime::currentDateTime().addSecs(-delay), current_info->start);
        // calculate duration in minutes between start and time of change
        double duration = diff_time.hour() * 60.0 + diff_time.minute() + diff_time.second() / 60.0;
        current_info->duration = duration;

        // calculate new prediction for this rider
        vector<double> durations_M;
        for (auto& info : _current_infos)
            // ignore first formation lap
            if (info.name == current_info->name && info.type == "M" && _current_infos.at(0).duration != info.duration)
                durations_M.push_back(info.duration);

        double mean;
        // in case of formation lap
        if (durations_M.size() == 0)
            mean = 53.0;
        else
            mean = std::accumulate(std::begin(durations_M), std::end(durations_M), 0.0) / durations_M.size();

        for (auto& info : _current_infos)
            // ignore first formation lap
            if (info.name == current_info->name && info.type == "P" && _current_infos.at(0).duration != info.duration)
                info.duration = mean;

       writeLatestInfos(QString::fromStdString(_path), _current_infos);

        showTable(_current_infos);
    }*/
}
// ...........................................................................
QTime RaceTeamMonitor::timeDiff(QDateTime time_1, QDateTime time_2)
// ...........................................................................
{
    // difference in seconds
    double diff = (time_1.toMSecsSinceEpoch() - time_2.toMSecsSinceEpoch()) / 1000.0;

    diff = fmod(diff, 86400.0);
    // conversion to hours, minutes and seconds
    double hours = abs(diff) / 3600.0;
    double hours_floor = floor(hours);
    double min_in_hours = hours - hours_floor;
    double minutes = min_in_hours * 60.0;
    double minutes_floor = floor(minutes);
    double seconds = (minutes - minutes_floor) * 60.0;

    QTime diff_time = QTime(hours_floor, minutes_floor, seconds);

    return diff_time;
}
// ...........................................................................
