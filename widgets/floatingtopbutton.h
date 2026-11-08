#ifndef FLOATINGTOPBUTTON_H
#define FLOATINGTOPBUTTON_H

#include <QWidget>

#include "ElaWidgetToolsDef.h"

class QScrollBar;

namespace GManager {

// 悬浮“回到顶部”按钮：半透明圆底 + 上箭头，不占布局，由页面自己定位
class FloatingTopButton : public QWidget {
  Q_OBJECT
public:
  explicit FloatingTopButton(QWidget *parent = nullptr);
  ~FloatingTopButton() override;

signals:
  void clicked();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  ElaThemeType::ThemeMode _themeMode;
};

// 平滑滚动到顶部：交给 QPropertyAnimation 驱动滚动条，保留平滑动画
void animateScrollToTop(QScrollBar *bar, QObject *context);

} // namespace GManager

#endif // !FLOATINGTOPBUTTON_H
