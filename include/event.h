#pragma once
// -------------------- UNDER DEVELOPMENT / NOT IN USE --------------------
#include <iostream>
#include <fstream>
#include <QDialog>
#include <QVBoxLayout>
#include <QRadioButton>
#include <QPlainTextEdit>
#include <filesystem>
#include <QDir>
#include "ui_event.h"

struct DriverInfo
{
    std::string name;
    double pre_lap_duration;
    std::string glympse_id;
    int change_interval;
};

struct EventInfo
{
    std::string name;
    QDateTime race_begin;
    double race_duration_hours;
    std::vector<DriverInfo> drivers;
    double lap_length;
    double formation_lap_diff;
    std::string data_path;
};

class Event : public QDialog
{
	Q_OBJECT

public:
	Event(QWidget *parent, EventInfo event);
	~Event();
    void updateDrivers();
    int checkedDriver();
    void writeConfigToFile(std::string cfg_file);

private:

	Ui::Event _ui;
    EventInfo _edit_event;
    int _rb_cnt;
    std::pair<int, QRadioButton*> _last_selected;
};
