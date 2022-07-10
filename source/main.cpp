#include "raceteammonitor.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    RaceTeamMonitor w;
    w.show();
    return a.exec();
}
