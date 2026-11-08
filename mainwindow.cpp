#include "mainwindow.h"

#include <QDebug>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QMouseEvent>

#include "ElaContentDialog.h"
#include "ElaMessageBar.h"

#include "database.h"
#include "pages/gamedetailpage.h"
#include "pages/home.h"
#include "pages/settingspage.h"

namespace GManager {

MainWindow::MainWindow(QWidget *parent) : ElaWindow(parent) {
  initWindow();

  // 中心窗口
  initContent();

  // 拦截默认关闭事件
  _closeDialog = new ElaContentDialog(this);
  connect(_closeDialog, &ElaContentDialog::rightButtonClicked, this,
          &MainWindow::close);
  connect(_closeDialog, &ElaContentDialog::middleButtonClicked, this, [=]() {
    _closeDialog->close();
    showMinimized();
  });
  this->setIsDefaultClosed(false);
  connect(this, &MainWindow::closeButtonClicked, this,
          [=]() { _closeDialog->exec(); });
}

MainWindow::~MainWindow() {}

void MainWindow::initWindow() {
  setFocusPolicy(Qt::StrongFocus);
  // 窗口图标
  setWindowIcon(QIcon(":/assets/images/gamepad.png"));
  resize(1200, 740);
  setMinimumSize(960, 540);

  // 标题栏按钮设置
  // 禁用置顶按钮
  setWindowButtonFlag(ElaAppBarType::StayTopButtonHint, false);

  // 窗口标题
  setWindowTitle("GManager");
  // 禁用 UserInfoCard 组件
  setUserInfoCardVisible(false);
}

void MainWindow::initContent() {
  _homePage = new Home(this);

  addPageNode("游戏库", _homePage, ElaIconType::House);

  _detailPage = new GameDetailPage(this);
  addPageNode("游戏详情", _detailPage, ElaIconType::Gamepad);
  _detailKey = _detailPage->property("ElaPageKey").toString();

  connect(_homePage, &Home::gameSelected, this, &MainWindow::showGameDetail);
  connect(_homePage, &Home::gameStatusChanged, this, [this](const QString &id) {
    if (!_detailPage || _detailPage->currentGameId() != id) {
      return;
    }
    GameInfo game;
    if (DatabaseManager::getInstance()->queryById(id, &game)) {
      _detailPage->setGame(game);
    }
  });
  connect(_homePage, &Home::gameDeleted, this, [this](const QString &id) {
    if (!_detailPage || _detailPage->currentGameId() != id) {
      return;
    }
    _detailPage->setGame(GameInfo{});
    navigation(_homePage->property("ElaPageKey").toString());
  });
  connect(_detailPage, &GameDetailPage::gameUpdated, this,
          [this](const QString &) { _homePage->refreshGames(); });
  connect(
      this, &MainWindow::navigationNodeClicked, this,
      [this](ElaNavigationType::NavigationNodeType, const QString &nodeKey) {
        if (nodeKey != _detailKey || !_detailPage) {
          return;
        }
        if (!_detailPage->hasGame()) {
          showRandomGameDetail();
        }
        // 每次进入详情页都从顶部开始显示
        _detailPage->scrollToTop();
      });
  connect(this, &MainWindow::userInfoCardClicked, this, [=]() {
    this->navigation(_homePage->property("ElaPageKey").toString());
  });

  _settingsPage = new SettingsPage(this);
  addFooterNode("设置", _settingsPage, _settingKey, 0,
                ElaIconType::GearComplex);
}

void MainWindow::showGameDetail(const QString &id) {
  GameInfo game;
  if (!DatabaseManager::getInstance()->queryById(id, &game)) {
    ElaMessageBar::warning(ElaMessageBarType::TopRight, "警告",
                           "未找到游戏信息", 2000);
    return;
  }

  if (!_detailPage) {
    return;
  }

  _detailPage->setGame(game);
  if (!_detailKey.isEmpty()) {
    navigation(_detailKey);
  }
  _detailPage->scrollToTop();
}

void MainWindow::showRandomGameDetail() {
  // 只取一条随机主键，避免为了随机展示把整个游戏库读进内存
  QString id;
  if (!DatabaseManager::getInstance()->queryRandomId(&id)) {
    ElaMessageBar::warning(ElaMessageBarType::TopRight, "警告",
                           "游戏库为空，请先添加游戏", 2000);
    return;
  }

  GameInfo game;
  if (!DatabaseManager::getInstance()->queryById(id, &game)) {
    ElaMessageBar::warning(ElaMessageBarType::TopRight, "警告",
                           "未找到游戏信息", 2000);
    return;
  }

  _detailPage->setGame(game);
  _detailPage->scrollToTop();
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
  if (getCurrentNavigationIndex() != 2) {
    switch (event->button()) {
    case Qt::BackButton: {
      this->setCurrentStackIndex(0);
      break;
    }
    case Qt::ForwardButton: {
      this->setCurrentStackIndex(1);
      break;
    }
    default: {
      break;
    }
    }
  }
  ElaWindow::mouseReleaseEvent(event);
}

} // namespace GManager
