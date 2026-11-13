#ifndef GAMEDETAILPAGE_H
#define GAMEDETAILPAGE_H

#include "ElaScrollPage.h"

#include <QImage>

#include "database.h"

class ElaIconButton;
class ElaText;
class QGridLayout;
class QLabel;
class QScrollArea;

namespace GManager {

class BannerBackground;
class FloatingTopButton;
class InfoField;
class NetworkTextBrowser;
class PlayStatusBadge;
class RatingBar;
class TagFlow;

// 游戏详情页：横幅 + 左侧富文本介绍 + 右侧信息列
class GameDetailPage : public ElaScrollPage {
  Q_OBJECT

public:
  explicit GameDetailPage(QWidget *parent = nullptr);
  ~GameDetailPage() override;

  bool hasGame() const;
  QString currentGameId() const;
  void setGame(const GameInfo &game);

  // 把页面滚动条复位到顶部
  void scrollToTop();

signals:
  // 修改游戏信息并成功写库后发出，供上层刷新游戏库列表
  void gameUpdated(const QString &id);

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  void updateBanner();
  void updateCover();
  void updateTexts();
  void updateLinks();

  void positionTopButton();
  void updateTopButton();
  void scrollToTopSmooth();

  void showBannerMenu(const QPoint &pos);
  void showCoverMenu();
  void showImageMenu(const QImage &image, bool banner);
  void editImage(bool banner);
  void editGameInfo();
  QImage currentCoverImage() const;

  GameInfo _game;
  // 封面当前使用的设备像素比：窗口换到不同缩放的屏幕时据此重建
  qreal _coverDpr{1.0};

  BannerBackground *_bannerBackground{nullptr};
  QLabel *_coverLabel{nullptr};
  ElaIconButton *_editButton{nullptr};
  ElaText *_titleText{nullptr};
  RatingBar *_ratingBar{nullptr};
  InfoField *_developerField{nullptr};
  InfoField *_categoriesField{nullptr};
  TagFlow *_tagsFlow{nullptr};
  ElaText *_linksText{nullptr};
  PlayStatusBadge *_playStatusBadge{nullptr};
  NetworkTextBrowser *_descriptionBrowser{nullptr};
  FloatingTopButton *_topButton{nullptr};
  QScrollArea *_scrollArea{nullptr};
  QGridLayout *_columnsGrid{nullptr};
  QWidget *_leftPanel{nullptr};
  QWidget *_rightPanel{nullptr};
  bool _compact{false};
};

} // namespace GManager

#endif // !GAMEDETAILPAGE_H
