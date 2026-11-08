#ifndef HOME_H
#define HOME_H

#include <QList>

#include "ElaScrollPage.h"

#include "database.h"

class ElaComboBox;
class ElaFlowLayout;
class ElaLineEdit;
class ElaToolButton;
class QResizeEvent;
class QScrollArea;
class QShowEvent;

namespace GManager {

class FloatingTopButton;
class GameCard;

class Home : public ElaScrollPage {
  Q_OBJECT
public:
  explicit Home(QWidget *parent = nullptr);
  ~Home();

  // 重新加载卡片列表；页面不可见时推迟到下次显示再刷新
  void refreshGames();

signals:
  void gameSelected(const QString &id);
  void gameStatusChanged(const QString &id);
  void gameDeleted(const QString &id);

protected:
  void showEvent(QShowEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  // 清空卡片并按当前筛选条件重新加载；keepLength 为 true 时恢复到已加载长度，
  // 让刷新（改信息/改状态/删游戏）后滚动位置不跳回顶部
  void reloadGames(bool keepLength = false);
  void clearCards();
  // 追加下一页数据，返回本次是否取到数据
  bool loadNextPage();
  // 滚动接近底部时预取下一页
  void maybeLoadNextPage();
  // 追加一条槽位；卡片由 updateVisibleCards() 按视口附近才创建
  void appendSlot(const GameSummary &summary);
  // 封面由 GameCard 自己投后台任务解码，这里只建控件
  GameCard *createCard(QWidget *slot, const GameSummary &summary);
  // 只为视口上下缓冲区内的槽位建卡片，其余释放，避免卡片无限累积
  void updateVisibleCards();
  void scheduleVisibleCardsUpdate();
  void recycleCard(int index);
  void showCardContextMenu(const QString &id, const QPoint &globalPos);
  void applySearch();
  // 悬浮“回到顶部”按钮的定位、显隐与平滑滚动
  void positionTopButton();
  void updateTopButton();
  void scrollToTopSmooth();

  ElaFlowLayout *_gameFlow{nullptr};
  ElaComboBox *_statusFilter{nullptr};
  QString _statusFilterValue;
  ElaLineEdit *_searchEdit{nullptr};
  ElaToolButton *_searchButton{nullptr};
  QString _keyword;
  QScrollArea *_scrollArea{nullptr};
  FloatingTopButton *_topButton{nullptr};
  // 三个列表等长：已加载数据、布局中的固定尺寸槽位、槽位里当前的卡片（nullptr
  // 表示已回收）
  QList<GameSummary> _summaries;
  QList<QWidget *> _slots;
  QList<GameCard *> _cards;
  // 已建卡片所在的连续下标区间；-1 表示当前没有卡片。window 之外的都回收
  int _firstCard{-1};
  int _lastCard{-1};
  bool _hasMore{false}; // 数据库里是否还有未加载的数据
  bool _loading{false}; // 防止重入（清空布局会触发滚动条信号）
  bool _pendingRefresh{false};
};

} // namespace GManager

#endif // !HOME_H
