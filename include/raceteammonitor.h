#pragma once

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream> 
#include <QTime>
#include <QTimer>
#include <QPushButton>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QWebEngineView>
#include <filesystem>

#include <QtWidgets/QMainWindow>
#include "ui_raceteammonitor.h"
#include "ui_orderchange.h"

#include "event.h"
#include "cpptoml.h"
#include "qcustomplot.h"

struct LapInfo
{
    std::string name; // e.g. Andi
    double duration;  // e.g. 50.000 minutes
    std::string type; // P for predicted, M for measured
    QDateTime start;  // individual start-time of lap
    QDateTime end;    // individual stop-time of lap 
};

class RaceTeamMonitor : public QMainWindow
{
    Q_OBJECT

public:

    RaceTeamMonitor(QWidget* parent = Q_NULLPTR);

    EventInfo parseEvent(std::string cfg_file);
    void initEvent();
    void createTable(std::vector<LapInfo> infos);

    QTime timeDiff(QDateTime time_1, QDateTime time_2);
    std::vector<LapInfo> readLatestInfos(QString path);
    std::vector<LapInfo> writeLatestInfos(QString path, std::vector<LapInfo> infos);

public slots:

    void clickedChange(); // driver changed after specific time

    void labelTicker();
    void tableTicker();
    void clocksTicker();
    void statsTicker();

private:

    Ui::RaceTeamMonitor _ui;

    EventInfo _event;
    int _current_idx;
    std::vector<LapInfo> _current_infos;
    std::map<std::string, int> _stats_mapper;
};



