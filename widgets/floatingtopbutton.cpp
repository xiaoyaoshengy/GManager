#include "floatingtopbutton.h"

#include <QPainter>
#include <QPropertyAnimation>
#include <QScrollBar>

#include "ElaIconButton.h"
#include "ElaTheme.h"

namespace GManager {

FloatingTopButton::FloatingTopButton(QWidget *parent)
    : QWidget(parent), _themeMode(eTheme->getThemeMode()) {
  setFixedSize(40, 40);
  setCursor(Qt::PointingHandCursor);
  setAttribute(Qt::WA_TranslucentBackground);
  ElaIconButton *button =
      new ElaIconButton(ElaIconType::ArrowUp, 16, 40, 40, this);
  button->setBorderRadius(20);
  button->setToolTip("回到顶部");
  button->setCursor(Qt::PointingHandCursor);
  connect(button, &QPushButton::clicked, this, &FloatingTopButton::clicked);
  connect(eTheme, &ElaTheme::themeModeChanged, this,
          [this](ElaThemeType::ThemeMode themeMode) {
            _themeMode = themeMode;
            update();
          });
}

FloatingTopButton::~FloatingTopButton() {}

void FloatingTopButton::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  QColor color = ElaThemeColor(_themeMode, BasicBase);
  color.setAlphaF(0.72);
  QColor border = ElaThemeColor(_themeMode, BasicBorder);
  border.setAlphaF(0.65);
  painter.setPen(QPen(border, 1));
  painter.setBrush(color);
  painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 20,
                          20);
}

void animateScrollToTop(QScrollBar *bar, QObject *context) {
  if (!bar) {
    return;
  }
  auto *animation = new QPropertyAnimation(bar, "value", context);
  animation->setDuration(250);
  animation->setStartValue(bar->value());
  animation->setEndValue(bar->minimum());
  animation->setEasingCurve(QEasingCurve::OutCubic);
  animation->start(QAbstractAnimation::DeleteWhenStopped);
}

} // namespace GManager
