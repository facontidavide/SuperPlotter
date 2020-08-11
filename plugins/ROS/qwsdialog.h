#ifndef QWSDIALOG_H
#define QWSDIALOG_H

#include <QDialog>
#include <ros/ros.h>

namespace Ui
{
class QWSDialog;
}

class QWSDialog : public QDialog
{
  Q_OBJECT

public:
  ~QWSDialog();
  explicit QWSDialog(QWidget* parent = 0);

  static bool Connect(const std::string& ros_master_uri, const std::string& hostname = "localhost");

private slots:
  void on_pushButtonConnect_pressed();

  void on_pushButtonCancel_pressed();

private:
  Ui::QWSDialog* ui;
};

#endif  // QWSDIALOG_H