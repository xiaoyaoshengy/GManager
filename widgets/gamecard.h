#ifndef GAMECARD_H
#define GAMECARD_H

#include <QWidget>

#include "ElaWidgetToolsDef.h"

namespace GManager {

class GameCard : public QWidget {
  Q_OBJECT
  Q_PROPERTY(qreal scaleFactor READ scaleFactor WRITE setScaleFactor)

public:
  explicit GameCard(QWidget *parent = nullptr);
  ~GameCard() override;

  void setCover(const QImage &cover);
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
  ElaThemeType::ThemeMode _themeMode; // 主题模式
  QImage _cover;                      // 封面
  QString _title;                     // 标题
  QString _playStatus;                // 游玩状态
  bool _drawBound;                    // 是否绘制边框
  qreal _scaleFactor;                 // 缩放系数
};

} // namespace GManager

#endif // !GAMECARD_H
