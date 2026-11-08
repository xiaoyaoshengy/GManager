#ifndef GAMECARD_H
#define GAMECARD_H

#include <QPixmap>
#include <QWidget>

#include "ElaWidgetToolsDef.h"

namespace GManager {

// 卡片尺寸：首页的槽位控件必须与它一致，否则回收/重建时布局会重排
constexpr int kGameCardWidth = 160;
constexpr int kGameCardHeight = 240;

class GameCard : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal scaleFactor READ scaleFactor WRITE setScaleFactor)

public:
  explicit GameCard(QWidget *parent = nullptr);
  ~GameCard() override;

  // 封面只存路径，缩略图经 QPixmapCache 取（未命中时后台线程解码），
  // 避免每张卡片各持一份原图；空路径表示默认封面
  void setCoverPath(const QString &coverPath);
  // 后台解码完成后回填缩略图
  void applyCover(const QPixmap &cover);
  void setTitle(const QString &title);
  void setPlayStatus(const QString &status);
  QString playStatus() const;

  qreal scaleFactor() const;
  void setScaleFactor(const qreal factor);

signals:
  void clicked();

protected:
  bool event(QEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  // 缓存命中就立刻生效，否则投一个后台解码任务，避免滚动时阻塞 UI
  void requestCover();

  ElaThemeType::ThemeMode _themeMode; // 主题模式
  QString _coverPath;                 // 封面文件路径，空表示默认封面
  QString _title;                     // 标题
  QString _playStatus;                // 游玩状态
  mutable QPixmap _cover;             // 缩略图副本（与缓存隐式共享）
  mutable bool _coverLoaded;          // 是否已拿到缩略图
  mutable bool _coverRequested;       // 是否已排过后台解码任务
  bool _drawBound;                    // 是否绘制边框
  qreal _scaleFactor;                 // 缩放系数
};

} // namespace GManager

#endif // !GAMECARD_H
