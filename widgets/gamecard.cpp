#include "gamecard.h"

#include <QEvent>
#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>

#include "ElaTheme.h"

// 卡片尺寸
#define CARD_WIDTH 160
#define CARD_HEIGHT 240

// 封面相关
#define COVER_HEIGHT 212 // 高度
#define BORDER_RADIUS 6  // 圆角半径

namespace GManager {

namespace {

ElaIconType::IconName playStatusIcon(const QString &status) {
  if (status == QStringLiteral("未游玩")) {
    return ElaIconType::CircleDot;
  }
  if (status == QStringLiteral("游玩中")) {
    return ElaIconType::CirclePlay;
  }
  if (status == QStringLiteral("已完成")) {
    return ElaIconType::CircleCheck;
  }
  if (status == QStringLiteral("搁置")) {
    return ElaIconType::CirclePause;
  }
  if (status == QStringLiteral("抛弃")) {
    return ElaIconType::CircleXmark;
  }
  return ElaIconType::None;
}

QColor playStatusColor(const QString &status) {
  if (status == QStringLiteral("游玩中")) {
    return QColor(QStringLiteral("#3B82F6"));
  }
  if (status == QStringLiteral("已完成")) {
    return QColor(QStringLiteral("#22C55E"));
  }
  if (status == QStringLiteral("搁置")) {
    return QColor(QStringLiteral("#F59E0B"));
  }
  if (status == QStringLiteral("抛弃")) {
    return QColor(QStringLiteral("#EF4444"));
  }
  return QColor(QStringLiteral("#8A8A8A"));
}

} // namespace

GameCard::GameCard(QWidget *parent) : QWidget(parent) {
  setFixedSize(CARD_WIDTH, CARD_HEIGHT);

  _themeMode = eTheme->getThemeMode();
  _cover = QImage(":/assets/images/buka.jpg");
  _title = QString("Untitled");
  _drawBound = false;
  _scaleFactor = 1;

  connect(eTheme, &ElaTheme::themeModeChanged, this,
          [=](ElaThemeType::ThemeMode themeMode) { _themeMode = themeMode; });
}

GameCard::~GameCard() {}

void GameCard::setCover(const QImage &cover) {
  _cover = cover;
  update();
}

void GameCard::setTitle(const QString &title) {
  _title = title;
  update();
}

void GameCard::setPlayStatus(const QString &status) {
  _playStatus = status;
  update();
}

QString GameCard::playStatus() const { return _playStatus; }

qreal GameCard::scaleFactor() const { return _scaleFactor; }

void GameCard::setScaleFactor(const qreal factor) { _scaleFactor = factor; }

bool GameCard::event(QEvent *event) {
  switch (event->type()) {
  case QEvent::Enter: {
    _drawBound = true;
    QPropertyAnimation *hoverAnimation =
        new QPropertyAnimation(this, "scaleFactor", this);
    connect(hoverAnimation, &QPropertyAnimation::valueChanged, this,
            [=]() { update(); });
    hoverAnimation->setDuration(130);
    hoverAnimation->setStartValue(_scaleFactor);
    hoverAnimation->setEndValue(0.9);
    hoverAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    break;
  }
  case QEvent::Leave: {
    _drawBound = false;
    QPropertyAnimation *hoverAnimation =
        new QPropertyAnimation(this, "scaleFactor", this);
    connect(hoverAnimation, &QPropertyAnimation::valueChanged, this,
            [=]() { update(); });
    hoverAnimation->setDuration(130);
    hoverAnimation->setStartValue(_scaleFactor);
    hoverAnimation->setEndValue(1);
    hoverAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    break;
  }
  default:
    break;
  }

  return QWidget::event(event);
}

void GameCard::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
    emit clicked();
  }
  QWidget::mouseReleaseEvent(event);
}

void GameCard::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHints(QPainter::SmoothPixmapTransform |
                         QPainter::Antialiasing | QPainter::TextAntialiasing);

  painter.save();
  painter.setPen(Qt::NoPen);

  // 封面区域
  QRect coverRect(0, 0, CARD_WIDTH, COVER_HEIGHT);

  // 圆角边框
  QPainterPath path;
  path.addRoundedRect(coverRect, BORDER_RADIUS, BORDER_RADIUS);
  painter.setClipPath(path);

  // 封面绘制
  qreal windowAspectRatio = (qreal)CARD_WIDTH / COVER_HEIGHT;
  qreal pixAspectRatio = (qreal)_cover.width() / _cover.height();
  int targetPixWidth, targetPixHeight;
  if (windowAspectRatio < pixAspectRatio) {
    targetPixWidth =
        qRound(_cover.width() * windowAspectRatio / pixAspectRatio);
    targetPixHeight = _cover.height();
  } else {
    targetPixWidth = _cover.width();
    targetPixHeight =
        qRound(_cover.height() * pixAspectRatio / windowAspectRatio);
  }
  // 添加动画支持
  targetPixWidth *= _scaleFactor;
  targetPixHeight *= _scaleFactor;
  painter.drawImage(coverRect, _cover,
                    QRect((_cover.width() - targetPixWidth) / 2,
                          (_cover.height() - targetPixHeight) / 2,
                          targetPixWidth, targetPixHeight));

  if (_drawBound) {
    QPen pen(Qt::gray);
    pen.setWidth(3);
    painter.setPen(pen);
    painter.drawRoundedRect(coverRect, BORDER_RADIUS, BORDER_RADIUS);
  }
  painter.restore();

  painter.save();
  const ElaIconType::IconName statusIcon = playStatusIcon(_playStatus);
  if (statusIcon != ElaIconType::None) {
    const QRect badgeRect(CARD_WIDTH - 26, 6, 20, 20);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(playStatusColor(_playStatus));
    painter.drawEllipse(badgeRect);

    QFont iconFont(QStringLiteral("ElaAwesome"));
    iconFont.setPixelSize(12);
    painter.setFont(iconFont);
    painter.setPen(Qt::white);
    painter.drawText(badgeRect, Qt::AlignCenter, QChar(statusIcon));
  }
  painter.restore();

  painter.save();
  painter.setPen(ElaThemeColor(_themeMode, BasicText));

  // 标题区域
  QRect titleRect(0, COVER_HEIGHT + 3, CARD_WIDTH,
                  CARD_HEIGHT - COVER_HEIGHT - 3);

  // 标题绘制
  QFont font = painter.font();
  font.setPixelSize(15);
  painter.setFont(font);
  // 文本过长自动省略
  QFontMetrics fontWidth(painter.font());
  QString title = fontWidth.elidedText(_title, Qt::ElideRight, CARD_WIDTH);
  painter.drawText(titleRect, Qt::AlignCenter, title);
  painter.restore();
}

} // namespace GManager
