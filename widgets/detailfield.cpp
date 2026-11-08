#include "detailfield.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QtMath>

#include "ElaText.h"
#include "ElaTheme.h"

namespace GManager {

namespace {

const int kCaptionPixelSize = 12;
const int kValuePixelSize = 13;
const int kHintPixelSize = 12;

const int kStarSize = 18;
const int kStarGap = 5;
const int kStarCount = 5;

const int kChipHeight = 24;
const int kChipPadding = 10;
const int kChipGapX = 8;
const int kChipGapY = 8;
const int kChipFontSize = 12;

const int kBadgeHeight = 24;
const int kBadgeFontSize = 12;

QColor starColor() { return QColor(0xF5, 0xA6, 0x23); }

// 五角星路径：外接矩形内绘制，外半径取矩形短边的一半
QPainterPath starPath(const QRectF &rect) {
  const QPointF center = rect.center();
  const qreal outer = qMin(rect.width(), rect.height()) / 2.0;
  const qreal inner = outer * 0.44;
  QPainterPath path;
  for (int i = 0; i < 10; ++i) {
    const qreal radius = (i % 2 == 0) ? outer : inner;
    const qreal angle = -M_PI_2 + i * M_PI / 5.0;
    const QPointF point(center.x() + radius * qCos(angle),
                        center.y() + radius * qSin(angle));
    if (i == 0) {
      path.moveTo(point);
    } else {
      path.lineTo(point);
    }
  }
  path.closeSubpath();
  return path;
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

} // namespace

CaptionLabel::CaptionLabel(const QString &text, QWidget *parent)
    : QLabel(text, parent), _themeMode(eTheme->getThemeMode()) {
  QFont labelFont = font();
  labelFont.setPixelSize(kCaptionPixelSize);
  labelFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.4);
  setFont(labelFont);
  setWordWrap(false);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  applyTheme();
  connect(eTheme, &ElaTheme::themeModeChanged, this,
          [this](ElaThemeType::ThemeMode themeMode) {
            _themeMode = themeMode;
            applyTheme();
          });
}

void CaptionLabel::applyTheme() {
  setStyleSheet(QString("color:%1;background:transparent;")
                    .arg(ElaThemeColor(_themeMode, BasicDetailsText).name()));
}

Separator::Separator(QWidget *parent)
    : QWidget(parent), _themeMode(eTheme->getThemeMode()) {
  setFixedHeight(1);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  connect(eTheme, &ElaTheme::themeModeChanged, this,
          [this](ElaThemeType::ThemeMode themeMode) {
            _themeMode = themeMode;
            update();
          });
}

void Separator::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QColor color = ElaThemeColor(_themeMode, BasicBorder);
  color.setAlphaF(0.85);
  QPainter painter(this);
  painter.fillRect(rect(), color);
}

SectionHeader::SectionHeader(const QString &title, QWidget *parent)
    : QWidget(parent), _title(title), _themeMode(eTheme->getThemeMode()) {
  setFixedHeight(26);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  connect(eTheme, &ElaTheme::themeModeChanged, this,
          [this](ElaThemeType::ThemeMode themeMode) {
            _themeMode = themeMode;
            update();
          });
}

QSize SectionHeader::sizeHint() const {
  QFont titleFont = font();
  titleFont.setPixelSize(17);
  titleFont.setWeight(QFont::DemiBold);
  const QFontMetrics metrics(titleFont);
  return QSize(14 + metrics.horizontalAdvance(_title), height());
}

void SectionHeader::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);

  painter.setPen(Qt::NoPen);
  painter.setBrush(ElaThemeColor(_themeMode, PrimaryNormal));
  const int barHeight = 16;
  const qreal barTop = (height() - barHeight) / 2.0;
  painter.drawRoundedRect(QRectF(0, barTop, 4, barHeight), 2, 2);

  QFont titleFont = font();
  titleFont.setPixelSize(17);
  titleFont.setWeight(QFont::DemiBold);
  painter.setFont(titleFont);
  painter.setPen(ElaThemeColor(_themeMode, BasicText));
  painter.drawText(QRectF(14, 0, width() - 14, height()),
                   Qt::AlignLeft | Qt::AlignVCenter, _title);
}

InfoField::InfoField(const QString &label, QWidget *parent) : QWidget(parent) {
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(3);
  layout->addWidget(new CaptionLabel(label, this));
  _value = new ElaText(this);
  _value->setTextPixelSize(kValuePixelSize);
  _value->setMinimumWidth(0);
  layout->addWidget(_value);
}

void InfoField::setValue(const QString &value) {
  const QString text = value.trimmed();
  _value->setText(text.isEmpty() ? QStringLiteral("暂无") : text);
}

RatingBar::RatingBar(QWidget *parent)
    : QWidget(parent), _themeMode(eTheme->getThemeMode()) {
  setFixedHeight(22);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  connect(eTheme, &ElaTheme::themeModeChanged, this,
          [this](ElaThemeType::ThemeMode themeMode) {
            _themeMode = themeMode;
            update();
          });
}

void RatingBar::setRating(const QString &rating) {
  const QString text = rating.trimmed();
  bool ok = false;
  const qreal value = text.toDouble(&ok);
  if (ok) {
    _rating = qBound(0.0, value, qreal(kStarCount));
    _text = QStringLiteral("%1 / %2").arg(_rating, 0, 'f', 1).arg(kStarCount);
  } else {
    _rating = -1.0;
    _text = text.isEmpty() ? QStringLiteral("暂无评分") : text;
  }
  updateGeometry();
  update();
}

QSize RatingBar::sizeHint() const {
  QFont textFont = font();
  textFont.setPixelSize(kHintPixelSize);
  const QFontMetrics metrics(textFont);
  int width = kStarCount * kStarSize + (kStarCount - 1) * kStarGap;
  width += 10 + metrics.horizontalAdvance(_text);
  return QSize(width, height());
}

void RatingBar::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);

  const qreal starTop = (height() - kStarSize) / 2.0;
  const QColor emptyColor = ElaThemeColor(_themeMode, BasicBorder);
  if (_rating >= 0.0) {
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < kStarCount; ++i) {
      const QRectF starRect(i * (kStarSize + kStarGap), starTop, kStarSize,
                            kStarSize);
      painter.setBrush(emptyColor);
      painter.drawPath(starPath(starRect));

      const qreal fill = qBound(0.0, _rating - i, 1.0);
      if (fill > 0.0) {
        painter.save();
        painter.setClipRect(QRectF(starRect.left(), starRect.top(),
                                   starRect.width() * fill, starRect.height()));
        painter.setBrush(starColor());
        painter.drawPath(starPath(starRect));
        painter.restore();
      }
    }
  }

  QFont textFont = font();
  textFont.setPixelSize(kHintPixelSize);
  painter.setFont(textFont);
  painter.setPen(ElaThemeColor(_themeMode, BasicDetailsText));
  const qreal textLeft =
      _rating >= 0.0 ? kStarCount * (kStarSize + kStarGap) - kStarGap + 8 : 0.0;
  painter.drawText(QRectF(textLeft, 0, width() - textLeft, height()),
                   Qt::AlignLeft | Qt::AlignVCenter, _text);
}

TagFlow::TagFlow(QWidget *parent)
    : QWidget(parent), _themeMode(eTheme->getThemeMode()) {
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  setFixedHeight(kChipHeight);
  connect(eTheme, &ElaTheme::themeModeChanged, this,
          [this](ElaThemeType::ThemeMode themeMode) {
            _themeMode = themeMode;
            relayout();
            update();
          });
}

void TagFlow::setTags(const QStringList &tags) {
  _tags.clear();
  for (const QString &tag : tags) {
    const QString text = tag.trimmed();
    if (!text.isEmpty()) {
      _tags.append(text);
    }
  }
  setToolTip(_tags.join(QStringLiteral("，")));
  relayout();
  update();
}

QFont TagFlow::chipFont() const {
  QFont font = this->font();
  font.setPixelSize(kChipFontSize);
  return font;
}

void TagFlow::relayout() {
  _chips.clear();
  const QFontMetrics metrics(chipFont());
  const int available = qMax(width(), 60);
  int x = 0;
  int y = 0;
  for (const QString &tag : _tags) {
    const int naturalWidth = metrics.horizontalAdvance(tag) + kChipPadding * 2;
    const int chipWidth = qMin(naturalWidth, available);
    if (x > 0 && x + chipWidth > available) {
      x = 0;
      y += kChipHeight + kChipGapY;
    }
    Chip chip;
    chip.text = tag;
    chip.rect = QRect(x, y, chipWidth, kChipHeight);
    _chips.append(chip);
    x += chipWidth + kChipGapX;
  }
  _contentHeight = _chips.isEmpty() ? 20 : y + kChipHeight;
  if (height() != _contentHeight) {
    setFixedHeight(_contentHeight);
  }
}

QSize TagFlow::sizeHint() const { return QSize(0, _contentHeight); }

void TagFlow::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  relayout();
}

void TagFlow::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);

  if (_chips.isEmpty()) {
    QFont emptyFont = font();
    emptyFont.setPixelSize(kValuePixelSize);
    painter.setFont(emptyFont);
    painter.setPen(ElaThemeColor(_themeMode, BasicDetailsText));
    painter.drawText(QRectF(0, 0, width(), _contentHeight),
                     Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("暂无"));
    return;
  }

  QColor chipBase = ElaThemeColor(_themeMode, BasicHoverAlpha);
  QColor chipBorder = ElaThemeColor(_themeMode, BasicBorder);
  chipBorder.setAlphaF(0.6);
  const QColor textColor = ElaThemeColor(_themeMode, BasicTextCategory);
  const QFontMetrics metrics(chipFont());
  painter.setFont(chipFont());

  for (const Chip &chip : _chips) {
    const QRectF chipRect(chip.rect);
    painter.setPen(QPen(chipBorder, 1));
    painter.setBrush(chipBase);
    painter.drawRoundedRect(chipRect.adjusted(0.5, 0.5, -0.5, -0.5),
                            chipRect.height() / 2.0, chipRect.height() / 2.0);

    // 只有真的放不下时才省略，避免正常宽度下也出现省略号
    QString text = chip.text;
    if (metrics.horizontalAdvance(text) + kChipPadding * 2 >
        chip.rect.width()) {
      text = metrics.elidedText(chip.text, Qt::ElideRight,
                                chip.rect.width() - kChipPadding * 2);
    }
    painter.setPen(textColor);
    painter.drawText(chipRect, Qt::AlignCenter, text);
  }
}

PlayStatusBadge::PlayStatusBadge(QWidget *parent) : QWidget(parent) {
  setFixedHeight(kBadgeHeight);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  setStatus(QString());
}

void PlayStatusBadge::setStatus(const QString &status) {
  _status = status.trimmed();
  setFixedWidth(sizeHint().width());
  update();
}

QSize PlayStatusBadge::sizeHint() const {
  QFont textFont = font();
  textFont.setPixelSize(kBadgeFontSize);
  textFont.setWeight(QFont::DemiBold);
  const QFontMetrics metrics(textFont);
  const QString text = _status.isEmpty() ? QStringLiteral("未游玩") : _status;
  const int width = 10 + 20 + metrics.horizontalAdvance(text) + 12;
  return QSize(width, kBadgeHeight);
}

void PlayStatusBadge::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);

  const QString status = _status.isEmpty() ? QStringLiteral("未游玩") : _status;
  const QColor color = playStatusColor(status);
  QColor background = color;
  background.setAlphaF(0.16);
  painter.setPen(QPen(color, 1));
  painter.setBrush(background);
  painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                          height() / 2.0, height() / 2.0);

  qreal left = 8;
  const ElaIconType::IconName icon = playStatusIcon(status);
  if (icon != ElaIconType::None) {
    QFont iconFont(QStringLiteral("ElaAwesome"));
    iconFont.setPixelSize(kBadgeFontSize);
    painter.setFont(iconFont);
    painter.setPen(color);
    painter.drawText(QRectF(left, 0, 16, height()), Qt::AlignCenter,
                     QChar(icon));
    left += 20;
  }

  QFont textFont = font();
  textFont.setPixelSize(kBadgeFontSize);
  textFont.setWeight(QFont::DemiBold);
  painter.setFont(textFont);
  painter.setPen(color);
  painter.drawText(QRectF(left, 0, width() - left - 6, height()),
                   Qt::AlignLeft | Qt::AlignVCenter, status);
}

} // namespace GManager
