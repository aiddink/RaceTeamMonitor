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

#include <QtWidgets/QMainWindow>
#include "ui_raceteammonitor.h"

#include "qcustomplot.h"

struct lapinfo
{
    std::string name; // e.g. Andi
    double duration; // e.g. 50.000 minutes
    std::string type; // P for predicted, M for measured
    QDateTime start;
    QDateTime end;
};

static std::vector<Qt::GlobalColor> marker_color = { Qt::red, Qt::green, Qt::blue, Qt::magenta };

class RaceTeamMonitor : public QMainWindow
{
    Q_OBJECT

public:
    RaceTeamMonitor(QWidget* parent = Q_NULLPTR);

    void showTable(std::vector<lapinfo> infos);

    std::vector<lapinfo> readLatestInfos(QString path);
    void writeLatestInfos(QString path, std::vector<lapinfo> infos);

public slots:

    void labelTicker();
    void tableTicker();
    void clocksTicker();
    void updateTable();

    void updateStatsTable();
    void openChangeDialog();
    QTime timeDiff(QDateTime time_1, QDateTime time_2);

private:
    Ui::RaceTeamMonitor _ui;
    std::string _path;
    std::vector<lapinfo> _current_infos;
    int _current_idx;

    //map<string, QCPGraph*> _id_graph;
    //QCPItemTracer* _itemDemoPhaseTracer;
   // QCPItemText* _cyclist_text;
};



