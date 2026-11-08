#ifndef HOME_H
#define HOME_H

#include "ElaScrollPage.h"

class ElaComboBox;
class ElaFlowLayout;
class ElaLineEdit;
class ElaToolButton;
class QShowEvent;

namespace GManager {

struct GameInfo;

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

private:
  void reloadGames();
  void showCardContextMenu(const QString &id, const QPoint &globalPos);
  void applySearch();
  bool matchesKeyword(const GameInfo &game) const;

  ElaFlowLayout *_gameFlow{nullptr};
  ElaComboBox *_statusFilter{nullptr};
  QString _statusFilterValue;
  ElaLineEdit *_searchEdit{nullptr};
  ElaToolButton *_searchButton{nullptr};
  QString _keyword;
  bool _pendingRefresh{false};
};

} // namespace GManager

#endif // !HOME_H
