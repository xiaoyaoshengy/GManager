#include "home.h"

#include <QAbstractItemView>
#include <QDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QLayoutItem>
#include <QVBoxLayout>

#include "ElaComboBox.h"
#include "ElaFlowLayout.h"
#include "ElaLineEdit.h"
#include "ElaMenu.h"
#include "ElaMessageBar.h"
#include "ElaText.h"
#include "ElaToolButton.h"

#include "addgamedialog.h"
#include "database.h"
#include "gamecard.h"
#include "global_defines.h"
#include "playstatus.h"

namespace GManager {

namespace {

ElaText *createHintText(const QString &text, QWidget *parent) {
  ElaText *hint = new ElaText(text, parent);
  hint->setTextPixelSize(13);
  return hint;
}

} // namespace

Home::Home(QWidget *parent) : ElaScrollPage(parent) {
  setContentsMargins(0, 20, 0, 0);
  setTitleVisible(false);

  // 推荐卡片
  ElaText *flowText = new ElaText("游戏", this);
  flowText->setTextPixelSize(30);
  flowText->setTextStyle(ElaTextType::Title);

  QHBoxLayout *flowTextLayout = new QHBoxLayout();
  flowTextLayout->setContentsMargins(33, 0, 33, 0);
  flowTextLayout->addWidget(flowText);
  flowTextLayout->addStretch();

  ElaToolButton *addGameButton = new ElaToolButton(this);
  addGameButton->setFixedHeight(35);
  addGameButton->setIsTransparent(false);
  addGameButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  addGameButton->setText("添加游戏");
  addGameButton->setElaIcon(ElaIconType::Plus);

  _statusFilter = new ElaComboBox(this);
  _statusFilter->addItem("全部状态", QString());
  for (const QString &status : playStatusOptions()) {
    _statusFilter->addItem(status, status);
  }
  _statusFilter->setFixedWidth(120);
  // 全部状态 + 五种状态，全部展开且不显示滚动条
  _statusFilter->setMaxVisibleItems(6);
  _statusFilter->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  _statusFilter->view()->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  _searchEdit = new ElaLineEdit(this);
  _searchEdit->setPlaceholderText("搜索游戏");
  _searchEdit->setFixedWidth(240);
  _searchEdit->setIsClearButtonEnable(true);

  _searchButton = new ElaToolButton(this);
  _searchButton->setFixedSize(35, 35);
  _searchButton->setIsTransparent(false);
  _searchButton->setElaIcon(ElaIconType::MagnifyingGlass);

  flowTextLayout->addWidget(_statusFilter);
  flowTextLayout->addSpacing(6);
  flowTextLayout->addWidget(_searchEdit);
  flowTextLayout->addSpacing(6);
  flowTextLayout->addWidget(_searchButton);
  flowTextLayout->addSpacing(10);
  flowTextLayout->addWidget(addGameButton);

  connect(_searchEdit, &QLineEdit::returnPressed, this, &Home::applySearch);
  connect(_searchButton, &QToolButton::clicked, this, &Home::applySearch);
  connect(_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) {
            _statusFilterValue = _statusFilter->currentData().toString();
            reloadGames();
          });

  connect(addGameButton, &ElaToolButton::clicked, this, [this]() {
    AddGameDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
      reloadGames();
      const QString id = dialog.addedGameId();
      if (!id.isEmpty()) {
        emit gameSelected(id);
      }
    }
  });

  _gameFlow = new ElaFlowLayout(0, 5, 5);
  _gameFlow->setContentsMargins(30, 0, 0, 0);
  _gameFlow->setIsAnimation(true);

  QWidget *centralWidget = new QWidget(this);
  QVBoxLayout *centerVLayout = new QVBoxLayout(centralWidget);
  centerVLayout->setSpacing(0);
  centerVLayout->setContentsMargins(0, 0, 0, 0);
  centerVLayout->addLayout(flowTextLayout);
  centerVLayout->addSpacing(20);
  centerVLayout->addLayout(_gameFlow);
  centerVLayout->addStretch();
  addCentralWidget(centralWidget);

  reloadGames();
}

Home::~Home() {}

void Home::refreshGames() {
  // 页面不可见时卡片流无法完成布局，推迟到下次显示再重建
  if (!isVisible()) {
    _pendingRefresh = true;
    return;
  }
  reloadGames();
}

void Home::showEvent(QShowEvent *event) {
  ElaScrollPage::showEvent(event);
  if (_pendingRefresh) {
    _pendingRefresh = false;
    reloadGames();
  }
}

void Home::reloadGames() {
  // 移除上一次渲染的卡片
  while (QLayoutItem *item = _gameFlow->takeAt(0)) {
    if (QWidget *widget = item->widget()) {
      widget->deleteLater();
    }
    delete item;
  }

  DatabaseManager *database = DatabaseManager::getInstance();
  if (!database->isReady()) {
    ElaMessageBar::error(ElaMessageBarType::TopRight, "错误",
                         "游戏数据库初始化失败", 2000);
    _gameFlow->addWidget(createHintText("游戏数据库初始化失败", this));
    return;
  }

  const QList<GameInfo> games = database->queryAll();
  const bool filtering = !_keyword.isEmpty() || !_statusFilterValue.isEmpty();
  QList<GameInfo> visibleGames;
  for (const GameInfo &game : games) {
    const bool matched =
        (_keyword.isEmpty() || matchesKeyword(game)) &&
        (_statusFilterValue.isEmpty() || game.playStatus == _statusFilterValue);
    if (matched) {
      visibleGames.append(game);
    }
  }
  if (visibleGames.isEmpty()) {
    _gameFlow->addWidget(
        createHintText(filtering ? "未找到相关游戏" : "暂无游戏", this));
    return;
  }

  for (const GameInfo &game : visibleGames) {
    GameCard *card = new GameCard(this);
    if (game.cover.isEmpty() || game.cover == defaultCoverKey()) {
      card->setCover(QImage(defaultCoverResource()));
    } else {
      const QImage cover(QString("%1/%2").arg(coverDir(), game.cover));
      if (!cover.isNull()) {
        card->setCover(cover);
      }
    }
    card->setTitle(game.title);
    card->setPlayStatus(game.playStatus);
    connect(card, &GameCard::clicked, this,
            [this, id = game.id]() { emit gameSelected(id); });
    card->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(card, &QWidget::customContextMenuRequested, this,
            [this, id = game.id, card](const QPoint &pos) {
              showCardContextMenu(id, card->mapToGlobal(pos));
            });
    _gameFlow->addWidget(card);
  }
}

void Home::applySearch() {
  _keyword = _searchEdit->text().trimmed();
  reloadGames();
}

bool Home::matchesKeyword(const GameInfo &game) const {
  if (_keyword.isEmpty()) {
    return true;
  }
  return game.title.contains(_keyword, Qt::CaseInsensitive) ||
         game.developer.contains(_keyword, Qt::CaseInsensitive) ||
         game.tags.contains(_keyword, Qt::CaseInsensitive);
}

void Home::showCardContextMenu(const QString &id, const QPoint &globalPos) {
  GameInfo game;
  if (!DatabaseManager::getInstance()->queryById(id, &game)) {
    ElaMessageBar::warning(ElaMessageBarType::TopRight, "警告",
                           "未找到游戏信息", 2000);
    return;
  }

  ElaMenu menu(this);

  QAction *editAction = menu.addAction("修改");
  connect(editAction, &QAction::triggered, this,
          [this, id]() { emit gameSelected(id); });

  ElaMenu *statusMenu = menu.addMenu("游玩状态设置");
  for (const QString &status : playStatusOptions()) {
    QAction *action = statusMenu->addAction(status);
    action->setCheckable(true);
    action->setChecked(status == game.playStatus);
    connect(action, &QAction::triggered, this, [this, id, status]() {
      if (DatabaseManager::getInstance()->updatePlayStatus(id, status)) {
        emit gameStatusChanged(id);
        reloadGames();
      }
    });
  }

  menu.addSeparator();
  QAction *removeAction = menu.addAction("删除");
  connect(removeAction, &QAction::triggered, this, [this, id]() {
    if (DatabaseManager::getInstance()->removeGame(id)) {
      emit gameDeleted(id);
      reloadGames();
    } else {
      ElaMessageBar::warning(ElaMessageBarType::TopRight, "错误",
                             "删除游戏失败", 2000);
    }
  });

  menu.exec(globalPos);
}

} // namespace GManager
