#include "event.h"
// -------------------- UNDER DEVELOPMENT / NOT IN USE --------------------
Event::Event(QWidget *parent, EventInfo event)
	: QDialog(parent)
{
	_ui.setupUi(this);

	_last_selected.first = -1;
	_last_selected.second = nullptr;
	_rb_cnt = 1;

	_edit_event = event;

	_ui.start_timeedit->setDateTime(_edit_event.race_begin);
	_ui.duration_spinbox->setValue(_edit_event.race_duration_hours);
	_ui.length_dspinbox->setValue(_edit_event.lap_length);
	_ui.f_length_dspinbox->setValue(_edit_event.lap_length - _edit_event.formation_lap_diff);
	_ui.drivers_spinbox->setValue(_edit_event.drivers.size());

	// prepare layout once
	_ui.drivers_groupbox->setLayout(new QVBoxLayout);

	// adjust number of drivers within groupbox
	QObject::connect(_ui.drivers_spinbox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
		[this]
		{
			_edit_event.drivers.resize(_ui.drivers_spinbox->value());
			updateDrivers();
		});

	updateDrivers();

	// still under development, so we hide all buttons etc.
	_ui.main_groupbox->hide();

	// show configuration within simple editor
	_ui.editor->setLineWrapMode(QPlainTextEdit::NoWrap);
	QTextDocument* doc = _ui.editor->document();
	QFont font = doc->defaultFont();
	font.setPointSize(10);
	font.setFamily("Monospace");
	doc->setDefaultFont(font);

	std::string line;
	std::ifstream inStream;
	inStream.open("konfiguration.toml", std::ios::in);
	while (std::getline(inStream, line))
		_ui.editor->appendPlainText(QString::fromStdString(line));
	inStream.close();
	_ui.editor->repaint();

	QDir dir(QString::fromStdString(_edit_event.data_path));
	dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
	dir.setSorting(QDir::Name | QDir::Reversed);
	dir.setNameFilters(QStringList() << "*.laps");

	// create initial laps-file if direcoty is empty
	// determine the number of possible laps
	if (!dir.entryInfoList().isEmpty())
	{
		_ui.editor->setDisabled(true);
		_ui.reset_pushbutton->setDisabled(true);
	}

	// reload default configuration to editor
	QObject::connect(_ui.delete_pushbutton, &QPushButton::clicked, this,
		[this]() {

			std::filesystem::remove_all(_edit_event.data_path);
			std::filesystem::create_directory(_edit_event.data_path);
		});

	// reload default configuration to editor
	QObject::connect(_ui.reset_pushbutton, &QPushButton::clicked, this,
		[&]() {

			_ui.editor->clear();
			std::string line;
			std::ifstream inStream;
			inStream.open("default.toml", std::ios::in);
			while (std::getline(inStream, line))
				_ui.editor->appendPlainText(QString::fromStdString(line));
			inStream.close();
			_ui.editor->repaint();
		});
}
// ...........................................................................
void Event::writeConfigToFile(std::string cfg_file)
// ...........................................................................
{
	std::ofstream out(cfg_file.c_str(), std::ios::trunc);
	out << _ui.editor->toPlainText().toStdString();
	out.close();
}
// ...........................................................................
Event::~Event()
// ...........................................................................
{
}
// ...........................................................................
void Event::updateDrivers()
// ...........................................................................
{
	QLayout* layout = _ui.drivers_groupbox->layout();

	// clear layout completely
	QLayoutItem* item;
	while ((item = layout->takeAt(0)) != 0)
	{
		if (item->widget())
			item->widget()->setParent(NULL);

		delete item;
	}

	// create new layout
	QRadioButton* last;
	for (auto& driver : _edit_event.drivers)
	{
		QRadioButton* rb = new QRadioButton();
		if (driver.name.empty())
			driver.name = "Fahrer_" + std::to_string(_rb_cnt++);

		rb->setText(QString::fromStdString(driver.name));
		layout->addWidget(rb);

		std::string name = driver.name;
		QObject::connect(rb, &QRadioButton::toggled,
			[this, rb, name](bool checked)
			{
				if (checked)
				{
					if (_last_selected.first >= 0 && _last_selected.first < _edit_event.drivers.size())
					{
						DriverInfo& last_driver = _edit_event.drivers.at(_last_selected.first);
						std::string check_name = _ui.name_lineedit->text().toStdString();
						if(check_name != last_driver.name)
							_last_selected.second->setText(QString::fromStdString(check_name));

						last_driver.name = check_name;
						last_driver.pre_lap_duration = _ui.expected_duration_spinbox->value();
						last_driver.change_interval = _ui.interval_spinbox->value();
						last_driver.glympse_id = _ui.glympse_lineedit->text().toStdString();
					}

					int checked_driver_idx = checkedDriver();
					DriverInfo& current_driver = _edit_event.drivers.at(checked_driver_idx);
					_ui.name_lineedit->setText(QString::fromStdString(current_driver.name));
					_ui.expected_duration_spinbox->setValue(current_driver.pre_lap_duration);
					_ui.interval_spinbox->setValue(current_driver.change_interval);
					_ui.glympse_lineedit->setText(QString::fromStdString(current_driver.glympse_id));

					_last_selected.first = checked_driver_idx;
					_last_selected.second = rb;
				}

			});
		last = rb;
	}

	layout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding));
}
// ...........................................................................
int Event::checkedDriver()
// ...........................................................................
{
	for (int i = 0; i < _ui.drivers_spinbox->value(); ++i)
	{
		QRadioButton* rb = (QRadioButton*)_ui.drivers_groupbox->layout()->itemAt(i)->widget();
		if (rb->isChecked())
		{
			for(int i=0; i<_edit_event.drivers.size(); ++i)
			{
				if (_edit_event.drivers.at(i).name == rb->text().toStdString())
					return i;
			}
		}
	}
	return -1;
}