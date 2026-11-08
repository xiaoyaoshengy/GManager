#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ElaWindow.h"

#include <QMainWindow>

class ElaContentDialog;

namespace GManager {

class Home;
class GameDetailPage;
class SettingsPage;

class MainWindow : public ElaWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  void initWindow();
  void initContent();

protected:
  virtual void mouseReleaseEvent(QMouseEvent *event) override;

private:
  void showGameDetail(const QString &id);
  void showRandomGameDetail();

  ElaContentDialog *_closeDialog{nullptr};
  Home *_homePage{nullptr};
  GameDetailPage *_detailPage{nullptr};
  SettingsPage *_settingsPage{nullptr};
  QString _detailKey{""};
  QString _settingKey{""};
};

} // namespace GManager

#endif // MAINWINDOW_H
