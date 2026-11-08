#include "gamecard.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QImageReader>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QPointer>
#include <QPropertyAnimation>
#include <QRunnable>
#include <QThreadPool>

#include "ElaTheme.h"

#include "global_defines.h"

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

// 交给解码器直接缩放：JPEG 会走 DCT 缩放，比整图解码后再缩快一个数量级。
// 返回值可能略大于目标尺寸，调用方还要再做一次精确缩放
QImage loadScaledCover(const QString &path, int width, int height) {
  QImageReader reader(path);
  reader.setAutoTransform(true);
  const QSize sourceSize = reader.size();
  if (sourceSize.isValid()) {
    reader.setScaledSize(sourceSize.scaled(QSize(width, height),
                                           Qt::KeepAspectRatioByExpanding));
  }
  return reader.read();
}

QString coverCacheKey(const QString &source, int width, int height) {
  return QStringLiteral("gm-card-cover:%1:%2x%3")
      .arg(source)
      .arg(width)
      .arg(height);
}

// 封面解码专用线程池：并发限制在 4，避免一次铺开几十个解码任务把内存顶高
QThreadPool *coverPool() {
  static QThreadPool pool;
  static const bool configured = []() {
    pool.setMaxThreadCount(4);
    return true;
  }();
  Q_UNUSED(configured)
  return &pool;
}

// 后台解码封面：读文件与缩放都在工作线程，完成后回 GUI 线程填进卡片。
// 卡片已被回收时 QPointer 为空，结果直接丢弃
class CoverLoadTask : public QRunnable {
public:
  CoverLoadTask(GameCard *card, QString source, QString cacheKey, int width,
                int height)
      : _card(card), _source(std::move(source)), _cacheKey(std::move(cacheKey)),
        _width(width), _height(height) {}

  void run() override {
    // 卡片滚出视口已被回收时没必要再解码，省掉一次读盘与缩放
    if (_card.isNull()) {
      return;
    }
    QImage image = loadScaledCover(_source, _width, _height);
    if (image.isNull() && _source != defaultCoverResource()) {
      // 封面文件丢失时退回默认封面
      image = loadScaledCover(defaultCoverResource(), _width, _height);
    }
    if (image.isNull()) {
      return;
    }
    const QImage scaled =
        image.scaled(_width, _height, Qt::KeepAspectRatioByExpanding,
                     Qt::SmoothTransformation);
    // context 用 QCoreApplication 而不是卡片本身：卡片可能已经被回收，
    // 用已销毁的 QObject 当 context 不安全；QPointer 在 GUI 线程里判空
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [_card = _card, scaled, _cacheKey = _cacheKey]() {
          if (!_card) {
            return;
          }
          QPixmap pixmap = QPixmap::fromImage(scaled);
          QPixmapCache::insert(_cacheKey, pixmap);
          _card->applyCover(pixmap);
        },
        Qt::QueuedConnection);
  }

private:
  QPointer<GameCard> _card;
  QString _source;
  QString _cacheKey;
  int _width;
  int _height;
};

} // namespace

GameCard::GameCard(QWidget *parent) : QWidget(parent) {
  setFixedSize(kGameCardWidth, kGameCardHeight);

  _themeMode = eTheme->getThemeMode();
  _title = QString("Untitled");
  _coverLoaded = false;
  _coverRequested = false;
  _drawBound = false;
  _scaleFactor = 1;

  connect(eTheme, &ElaTheme::themeModeChanged, this,
          [=](ElaThemeType::ThemeMode themeMode) { _themeMode = themeMode; });
}

GameCard::~GameCard() {}

void GameCard::setCoverPath(const QString &coverPath) {
  _coverPath = coverPath;
  // 换封面后要重新取缩略图
  _coverLoaded = false;
  _coverRequested = false;
  _cover = QPixmap();
  requestCover();
  update();
}

void GameCard::requestCover() {
  if (_coverLoaded || _coverRequested) {
    return;
  }
  const qreal ratio = devicePixelRatioF();
  const int targetWidth = qMax(1, qRound(kGameCardWidth * ratio));
  const int targetHeight = qMax(1, qRound(COVER_HEIGHT * ratio));
  const QString source =
      _coverPath.isEmpty() ? defaultCoverResource() : _coverPath;
  const QString key = coverCacheKey(source, targetWidth, targetHeight);

  // 命中缓存时同步生效，开销只有一次哈希查找
  QPixmap cached;
  if (QPixmapCache::find(key, &cached)) {
    _cover = cached;
    _coverLoaded = true;
    return;
  }

  _coverRequested = true;
  coverPool()->start(
      new CoverLoadTask(this, source, key, targetWidth, targetHeight));
}

void GameCard::applyCover(const QPixmap &cover) {
  if (_coverLoaded) {
    return;
  }
  _cover = cover;
  _coverLoaded = true;
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
  QRect coverRect(0, 0, kGameCardWidth, COVER_HEIGHT);

  // 圆角边框
  QPainterPath path;
  path.addRoundedRect(coverRect, BORDER_RADIUS, BORDER_RADIUS);
  painter.setClipPath(path);

  // 封面绘制：读成员而不是 coverPixmap()，后者可能触发同步解码，
  // 滚动时会把 UI 卡住；封面还在后台解码就先画一块底色占位
  if (!_cover.isNull()) {
    const QPixmap &cover = _cover;
    const qreal windowAspectRatio = (qreal)kGameCardWidth / COVER_HEIGHT;
    const qreal pixAspectRatio = (qreal)cover.width() / cover.height();
    qreal targetPixWidth, targetPixHeight;
    if (windowAspectRatio < pixAspectRatio) {
      targetPixWidth = cover.width() * windowAspectRatio / pixAspectRatio;
      targetPixHeight = cover.height();
    } else {
      targetPixWidth = cover.width();
      targetPixHeight = cover.height() * pixAspectRatio / windowAspectRatio;
    }
    // 添加动画支持
    targetPixWidth *= _scaleFactor;
    targetPixHeight *= _scaleFactor;
    painter.drawPixmap(coverRect, cover,
                       QRectF((cover.width() - targetPixWidth) / 2,
                              (cover.height() - targetPixHeight) / 2,
                              targetPixWidth, targetPixHeight));
  } else {
    painter.setBrush(ElaThemeColor(_themeMode, BasicBase));
    painter.drawRoundedRect(coverRect, BORDER_RADIUS, BORDER_RADIUS);
  }

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
    const QRect badgeRect(kGameCardWidth - 26, 6, 20, 20);
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
  QRect titleRect(0, COVER_HEIGHT + 3, kGameCardWidth,
                  kGameCardHeight - COVER_HEIGHT - 3);

  // 标题绘制
  QFont font = painter.font();
  font.setPixelSize(15);
  painter.setFont(font);
  // 文本过长自动省略
  QFontMetrics fontWidth(painter.font());
  QString title = fontWidth.elidedText(_title, Qt::ElideRight, kGameCardWidth);
  painter.drawText(titleRect, Qt::AlignCenter, title);
  painter.restore();
}

} // namespace GManager
