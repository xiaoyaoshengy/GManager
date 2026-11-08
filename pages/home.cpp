#include "home.h"

#include <QAbstractItemView>
#include <QDialog>
#include <QHBoxLayout>
#include <QLayoutItem>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
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
#include "floatingtopbutton.h"
#include "gamecard.h"
#include "global_defines.h"
#include "playstatus.h"

namespace GManager {

namespace {

// 每一页的卡片数：一屏大约两行，预取多页可以平滑滚动
constexpr int kPageSize = 50;

// 视口上下各预留的卡片行数：越大启动与滚动时预先建好的卡片越多，
// 滚动越不容易看到空白，代价是常驻内存的 GameCard 更多
constexpr int kVisibleBufferRows = 8;

// 每轮最多新建的卡片数：首次填充一次建够，滚动中则小批多次，
// 避免一帧里集中解码多张封面造成掉帧
constexpr int kFirstFillCardsPerPass = 24;
constexpr int kScrollCardsPerPass = 6;

ElaText *createHintText(const QString &text, QWidget *parent) {
  ElaText *hint = new ElaText(text, parent);
  hint->setTextPixelSize(13);
  return hint;
}

} // namespace

Home::Home(QWidget *parent) : ElaScrollPage(parent) {
  setContentsMargins(0, 20, 0, 0);
  setTitleVisible(false);

  // 标题行固定在页面顶部，不随卡片流滚动
  QWidget *header = new QWidget(this);
  QHBoxLayout *headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(33, 0, 33, 20);
  headerLayout->setSpacing(0);

  // 推荐卡片
  ElaText *flowText = new ElaText("游戏", header);
  flowText->setTextPixelSize(30);
  flowText->setTextStyle(ElaTextType::Title);

  headerLayout->addWidget(flowText);
  headerLayout->addStretch();

  ElaToolButton *addGameButton = new ElaToolButton(header);
  addGameButton->setFixedHeight(35);
  addGameButton->setIsTransparent(false);
  addGameButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  addGameButton->setText("添加游戏");
  addGameButton->setElaIcon(ElaIconType::Plus);

  _statusFilter = new ElaComboBox(header);
  _statusFilter->addItem("全部状态", QString());
  for (const QString &status : playStatusOptions()) {
    _statusFilter->addItem(status, status);
  }
  _statusFilter->setFixedWidth(120);
  // 全部状态 + 五种状态，全部展开且不显示滚动条
  _statusFilter->setMaxVisibleItems(6);
  _statusFilter->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  _statusFilter->view()->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  _searchEdit = new ElaLineEdit(header);
  _searchEdit->setPlaceholderText("搜索游戏");
  _searchEdit->setFixedWidth(240);
  _searchEdit->setIsClearButtonEnable(true);

  _searchButton = new ElaToolButton(header);
  _searchButton->setFixedSize(35, 35);
  _searchButton->setIsTransparent(false);
  _searchButton->setElaIcon(ElaIconType::MagnifyingGlass);

  headerLayout->addWidget(_statusFilter);
  headerLayout->addSpacing(6);
  headerLayout->addWidget(_searchEdit);
  headerLayout->addSpacing(6);
  headerLayout->addWidget(_searchButton);
  headerLayout->addSpacing(10);
  headerLayout->addWidget(addGameButton);

  // 放到滚动区外，滑到卡片深处时搜索与筛选仍然可用
  setTopCustomWidget(header);

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
  centerVLayout->addLayout(_gameFlow);
  centerVLayout->addStretch();
  addCentralWidget(centralWidget);

  // 滚动接近底部时按需追加下一页，避免一次性创建全部卡片
  _scrollArea = findChild<QScrollArea *>();
  if (_scrollArea) {
    connect(_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int) {
              maybeLoadNextPage();
              updateTopButton();
              updateVisibleCards();
            });
  }

  _topButton = new FloatingTopButton(this);
  _topButton->hide();
  connect(_topButton, &FloatingTopButton::clicked, this,
          [this]() { scrollToTopSmooth(); });

  reloadGames();
}

Home::~Home() {}

void Home::refreshGames() {
  // 页面不可见时卡片流无法完成布局，推迟到下次显示再重建
  if (!isVisible()) {
    _pendingRefresh = true;
    return;
  }
  reloadGames(true);
}

void Home::showEvent(QShowEvent *event) {
  ElaScrollPage::showEvent(event);
  if (_pendingRefresh) {
    _pendingRefresh = false;
    reloadGames(true);
  }
}

void Home::resizeEvent(QResizeEvent *event) {
  ElaScrollPage::resizeEvent(event);
  positionTopButton();
  // 视口高度变了，可见范围需要重算
  updateVisibleCards();
}

// 悬浮按钮贴在右下角
void Home::positionTopButton() {
  if (!_topButton) {
    return;
  }
  constexpr int kMargin = 24;
  _topButton->move(width() - _topButton->width() - kMargin,
                   height() - _topButton->height() - kMargin);
  _topButton->raise();
}

// 向下滚动一定距离才显示，回到顶部后隐藏
void Home::updateTopButton() {
  if (!_topButton || !_scrollArea) {
    return;
  }
  const bool visible = _scrollArea->verticalScrollBar()->value() > 120;
  if (_topButton->isVisible() != visible) {
    _topButton->setVisible(visible);
  }
  if (visible) {
    _topButton->raise();
  }
}

// 平滑滚动到顶部；滚动区不可用时忽略
void Home::scrollToTopSmooth() {
  if (!_scrollArea) {
    return;
  }
  animateScrollToTop(_scrollArea->verticalScrollBar(), this);
}

void Home::reloadGames(bool keepLength) {
  // 刷新（改信息/改状态/删游戏）时保留已加载长度，避免滚动位置跳回顶部
  const int previousCount = keepLength ? _summaries.size() : 0;

  // 清空/重建过程中滚动条会发出 valueChanged，先挡住按需加载
  _loading = true;
  clearCards();
  _hasMore = true;

  DatabaseManager *database = DatabaseManager::getInstance();
  if (!database->isReady()) {
    _hasMore = false;
    _loading = false;
    ElaMessageBar::error(ElaMessageBarType::TopRight, "错误",
                         "游戏数据库初始化失败", 2000);
    _gameFlow->addWidget(createHintText("游戏数据库初始化失败", this));
    return;
  }

  _loading = false;
  loadNextPage();
  while (_summaries.size() < previousCount && loadNextPage()) {
  }
  scheduleVisibleCardsUpdate();
}

void Home::clearCards() {
  // 只删布局项，槽位由下面的统一 deleteLater 释放（卡片是槽位的子控件）
  QList<QWidget *> widgets;
  while (QLayoutItem *item = _gameFlow->takeAt(0)) {
    if (QWidget *widget = item->widget()) {
      widgets.append(widget);
    }
    delete item;
  }
  for (QWidget *widget : widgets) {
    widget->deleteLater();
  }
  _summaries.clear();
  _slots.clear();
  _cards.clear();
  _firstCard = -1;
  _lastCard = -1;
}

bool Home::loadNextPage() {
  if (_loading || !_hasMore) {
    return false;
  }
  _loading = true;

  GameFilter filter;
  filter.keyword = _keyword;
  filter.playStatus = _statusFilterValue;

  bool hasMore = false;
  const QList<GameSummary> summaries =
      DatabaseManager::getInstance()->querySummaries(filter, _summaries.size(),
                                                     kPageSize, &hasMore);
  _hasMore = hasMore;

  const bool filtering = !_keyword.isEmpty() || !_statusFilterValue.isEmpty();
  if (summaries.isEmpty() && _summaries.isEmpty()) {
    _gameFlow->addWidget(
        createHintText(filtering ? "未找到相关游戏" : "暂无游戏", this));
  }
  for (const GameSummary &summary : summaries) {
    appendSlot(summary);
  }

  _loading = false;

  // 卡片不足一屏时滚动条没有可滚动区间，不会触发 valueChanged，
  // 这里主动补一页，保证内容始终填满视口
  QTimer::singleShot(0, this, [this]() {
    if (!_scrollArea) {
      return;
    }
    if (_hasMore && _scrollArea->verticalScrollBar()->maximum() <= 0) {
      loadNextPage();
    }
  });
  scheduleVisibleCardsUpdate();
  return !summaries.isEmpty();
}

void Home::maybeLoadNextPage() {
  if (_loading || !_hasMore || !_scrollArea) {
    return;
  }
  QScrollBar *bar = _scrollArea->verticalScrollBar();
  // 距底部不足一屏时提前取下一页
  if (bar->maximum() - bar->value() > bar->pageStep()) {
    return;
  }
  loadNextPage();
}

void Home::appendSlot(const GameSummary &summary) {
  // 槽位尺寸与卡片完全一致，回收/重建时布局不会重排
  QWidget *slot = new QWidget(this);
  slot->setFixedSize(kGameCardWidth, kGameCardHeight);
  _summaries.append(summary);
  _slots.append(slot);
  _cards.append(nullptr);
  _gameFlow->addWidget(slot);
}

GameCard *Home::createCard(QWidget *slot, const GameSummary &summary) {
  GameCard *card = new GameCard(slot);
  if (summary.cover.isEmpty() || summary.cover == defaultCoverKey()) {
    card->setCoverPath(QString());
  } else {
    card->setCoverPath(QString("%1/%2").arg(coverDir(), summary.cover));
  }
  card->setTitle(summary.title);
  card->setPlayStatus(summary.playStatus);
  connect(card, &GameCard::clicked, this,
          [this, id = summary.id]() { emit gameSelected(id); });
  card->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(card, &QWidget::customContextMenuRequested, this,
          [this, id = summary.id, card](const QPoint &pos) {
            showCardContextMenu(id, card->mapToGlobal(pos));
          });
  card->move(0, 0);
  card->show();
  return card;
}

void Home::updateVisibleCards() {
  if (!_scrollArea || _slots.isEmpty()) {
    return;
  }
  // 视口上下按行预留，缓冲越宽滚动时越不容易看到空白
  const int buffer = kGameCardHeight * kVisibleBufferRows;

  QWidget *viewport = _scrollArea->viewport();
  const int viewportHeight = viewport->height();

  // 槽位按行铺排，纵坐标单调不减：二分定位首个进入上缓冲的槽位，
  // 避免每次滚动都把所有槽位换算一遍坐标（槽位会增长到上千个）
  int low = 0;
  int high = int(_slots.size()) - 1;
  int begin = int(_slots.size());
  while (low <= high) {
    const int mid = (low + high) / 2;
    const QWidget *slot = _slots.at(mid);
    const int top = slot->mapTo(viewport, QPoint(0, 0)).y();
    if (top + slot->height() >= -buffer) {
      begin = mid;
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  // 从 begin 往后扫到下缓冲区之外，窗口内只需要一次坐标换算
  int end = begin;
  while (end < _slots.size()) {
    const QWidget *slot = _slots.at(end);
    if (slot->mapTo(viewport, QPoint(0, 0)).y() > viewportHeight + buffer) {
      break;
    }
    ++end;
  }

  // 回收窗口外的卡片：已建卡片总在连续区间 [_firstCard, _lastCard] 内
  int recycled = 0;
  for (int i = _firstCard; i >= 0 && i <= _lastCard && i < _cards.size(); ++i) {
    if (i >= begin && i < end) {
      continue;
    }
    if (_cards.at(i)) {
      recycleCard(i);
      ++recycled;
    }
  }

  // 首次填充一次多建一些；滚动中每次少建几批，
  // 封面滚动时走后台解码，这里只控制控件创建量
  const bool firstFill = (_firstCard < 0);
  const int passLimit =
      firstFill ? kFirstFillCardsPerPass : kScrollCardsPerPass;
  int created = 0;
  int newFirst = -1;
  int newLast = -1;
  for (int i = begin; i < end; ++i) {
    if (!_cards.at(i)) {
      if (created >= passLimit) {
        // 剩下的交给下一轮事件循环，先把区间收紧到已建的卡片
        _firstCard = newFirst;
        _lastCard = newLast;
        scheduleVisibleCardsUpdate();
        return;
      }
      _cards[i] = createCard(_slots.at(i), _summaries.at(i));
      ++created;
    }
    if (newFirst < 0) {
      newFirst = i;
    }
    newLast = i;
  }
  _firstCard = newFirst;
  _lastCard = newLast;
}

void Home::scheduleVisibleCardsUpdate() {
  // 刚追加的槽位还没参与布局，等这一轮布局完成后再算可见范围
  QTimer::singleShot(0, this, &Home::updateVisibleCards);
}

void Home::recycleCard(int index) {
  GameCard *card = _cards.at(index);
  if (!card) {
    return;
  }
  _cards[index] = nullptr;
  card->hide();
  card->deleteLater();
}

void Home::applySearch() {
  _keyword = _searchEdit->text().trimmed();
  reloadGames();
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
        reloadGames(true);
      }
    });
  }

  menu.addSeparator();
  QAction *removeAction = menu.addAction("删除");
  connect(removeAction, &QAction::triggered, this, [this, id]() {
    if (DatabaseManager::getInstance()->removeGame(id)) {
      emit gameDeleted(id);
      reloadGames(true);
    } else {
      ElaMessageBar::warning(ElaMessageBarType::TopRight, "错误",
                             "删除游戏失败", 2000);
    }
  });

  menu.exec(globalPos);
}

} // namespace GManager
