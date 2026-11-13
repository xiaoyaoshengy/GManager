#include "gamedetailpage.h"

#include <QCursor>
#include <QDir>
#include <QEasingCurve>
#include <QEvent>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include "ElaIconButton.h"
#include "ElaMenu.h"
#include "ElaMessageBar.h"
#include "ElaText.h"
#include "ElaTheme.h"

#include "detailfield.h"
#include "editgamedialog.h"
#include "floatingtopbutton.h"
#include "global_defines.h"
#include "imageeditdialog.h"
#include "imagerender.h"
#include "imageviewerdialog.h"
#include "networktextbrowser.h"
#include "playstatus.h"

namespace GManager {

namespace {

const int kBannerContentTop = 800;

// 标签字符串按常见分隔符拆分，去空去重
QStringList splitTags(const QString &value) {
  static const QRegularExpression separator(QStringLiteral("[、,，;；/|]"));
  QStringList tags;
  for (const QString &part : value.split(separator, Qt::SkipEmptyParts)) {
    const QString tag = part.trimmed();
    if (!tag.isEmpty() && !tags.contains(tag)) {
      tags.append(tag);
    }
  }
  return tags;
}

void showWarning(const QString &message) {
  ElaMessageBar::warning(ElaMessageBarType::TopRight, "警告", message, 2000);
}

// 判断可编辑字段是否发生变化（忽略封面、横幅等图片字段）
bool hasEditableChanges(const GameInfo &before, const GameInfo &after) {
  return before.title.trimmed() != after.title.trimmed() ||
         before.description.trimmed() != after.description.trimmed() ||
         before.rating.trimmed() != after.rating.trimmed() ||
         before.developer.trimmed() != after.developer.trimmed() ||
         before.tags.trimmed() != after.tags.trimmed() ||
         before.categories.trimmed() != after.categories.trimmed() ||
         before.links.trimmed() != after.links.trimmed() ||
         before.playStatus.trimmed() != after.playStatus.trimmed();
}

} // namespace

// 顶部背景：按图片宽高比绘制，并把内容起始位置以下逐渐虚化为透明
class BannerBackground : public QWidget {
public:
  explicit BannerBackground(int contentTop, QWidget *parent = nullptr)
      : QWidget(parent), _contentTop(contentTop) {}

  void setImage(const QImage &image) {
    _image = image;
    _cache = QPixmap();
    update();
  }

  const QImage &image() const { return _image; }

  int imageDisplayHeight() const {
    if (_image.isNull() || width() <= 0) {
      return 0;
    }
    return qMin(height(), qRound(qreal(width()) * _image.height() /
                                 qMax(1, _image.width())));
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    Q_UNUSED(event);
    if (_image.isNull()) {
      return;
    }
    const int width = this->width();
    const int height = imageDisplayHeight();
    if (width <= 0 || height <= 0) {
      return;
    }

    // 缓存按物理像素保存：尺寸或屏幕缩放比变化都要重建，否则把窗口拖到
    // 另一块不同缩放的屏幕后会继续复用旧密度的图
    const qreal dpr = qMax<qreal>(devicePixelRatioF(), 1.0);
    const QSize deviceTarget = deviceSize(QSize(width, height), dpr);
    if (_cache.isNull() || !qFuzzyCompare(_cache.devicePixelRatio(), dpr) ||
        _cache.size() != deviceTarget) {
      rebuildCache(deviceTarget, dpr);
    }
    QPainter painter(this);
    // dpr 为小数时物理尺寸取整后仍有亚像素级缩放，平滑兜底
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawPixmap(0, 0, _cache);
  }

private:
  // deviceTarget 是物理像素尺寸；虚化渐变也在物理像素上做，
  // 否则先按逻辑尺寸虚化再被放大，渐变边缘会齿化
  void rebuildCache(const QSize &deviceTarget, qreal dpr) {
    QImage scaled = _image
                        .scaled(deviceTarget, Qt::IgnoreAspectRatio,
                                Qt::SmoothTransformation)
                        .convertToFormat(QImage::Format_ARGB32_Premultiplied);
    {
      QPainter painter(&scaled);
      const int contentTop = qRound(_contentTop * dpr);
      if (contentTop < scaled.height()) {
        painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        QLinearGradient fade(0, contentTop, 0, scaled.height());
        fade.setColorAt(0.0, QColor(0, 0, 0, 255));
        fade.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.fillRect(scaled.rect(), fade);
      }
    }
    scaled.setDevicePixelRatio(dpr);
    _cache = QPixmap::fromImage(scaled);
  }

  QImage _image;
  QPixmap _cache;
  int _contentTop{0};
};

// 内容面板：半透明主题底色，保证文字与背景图的对比度
class ContentPanel : public QWidget {
public:
  explicit ContentPanel(QWidget *parent = nullptr)
      : QWidget(parent), _themeMode(eTheme->getThemeMode()) {
    setContextMenuPolicy(Qt::PreventContextMenu);
    connect(eTheme, &ElaTheme::themeModeChanged, this,
            [this](ElaThemeType::ThemeMode themeMode) {
              _themeMode = themeMode;
              update();
            });
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    // 提高底色不透明度，让文字在背景图上更清晰
    QColor color = ElaThemeColor(_themeMode, BasicBase);
    color.setAlphaF(0.72);
    QColor border = ElaThemeColor(_themeMode, BasicBorder);
    border.setAlphaF(0.65);
    painter.setPen(QPen(border, 1));
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 12,
                            12);
  }

private:
  ElaThemeType::ThemeMode _themeMode;
};

GameDetailPage::GameDetailPage(QWidget *parent) : ElaScrollPage(parent) {
  setTitleVisible(false);
  setContentsMargins(0, 0, 0, 0);

  _bannerBackground = new BannerBackground(kBannerContentTop, this);
  BannerBackground *central = _bannerBackground;
  central->setMinimumSize(0, 0);
  QVBoxLayout *rootLayout = new QVBoxLayout(central);
  rootLayout->setContentsMargins(30, kBannerContentTop, 30, 20);
  rootLayout->setSpacing(16);

  _leftPanel = new ContentPanel(central);
  _leftPanel->setMinimumWidth(0);
  _leftPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  QVBoxLayout *leftLayout = new QVBoxLayout(_leftPanel);
  leftLayout->setContentsMargins(20, 18, 20, 18);
  leftLayout->setSpacing(12);

  QHBoxLayout *introHeader = new QHBoxLayout();
  introHeader->setContentsMargins(0, 0, 0, 0);
  introHeader->setSpacing(8);
  introHeader->addWidget(new SectionHeader("游戏介绍", _leftPanel));
  introHeader->addStretch();
  _editButton =
      new ElaIconButton(ElaIconType::PenToSquare, 16, 28, 28, _leftPanel);
  _editButton->setToolTip("修改游戏信息");
  _editButton->setCursor(Qt::PointingHandCursor);
  introHeader->addWidget(_editButton);
  connect(_editButton, &QPushButton::clicked, this,
          &GameDetailPage::editGameInfo);

  leftLayout->addLayout(introHeader);
  leftLayout->addWidget(new Separator(_leftPanel));
  _descriptionBrowser = new NetworkTextBrowser(_leftPanel);
  _descriptionBrowser->setMinimumWidth(0);
  leftLayout->addWidget(_descriptionBrowser);
  leftLayout->addStretch();

  _rightPanel = new ContentPanel(central);
  _rightPanel->setMinimumWidth(0);
  _rightPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  QVBoxLayout *rightLayout = new QVBoxLayout(_rightPanel);
  rightLayout->setContentsMargins(20, 18, 20, 18);
  rightLayout->setSpacing(10);

  _coverLabel = new QLabel(_rightPanel);
  _coverLabel->setFixedSize(160, 240);
  _coverLabel->setAlignment(Qt::AlignCenter);
  rightLayout->addWidget(_coverLabel, 0, Qt::AlignHCenter);

  _titleText = new ElaText(_rightPanel);
  _titleText->setTextStyle(ElaTextType::Subtitle);
  _titleText->setAlignment(Qt::AlignHCenter);
  _titleText->setMinimumWidth(0);
  rightLayout->addWidget(_titleText);

  _ratingBar = new RatingBar(_rightPanel);
  rightLayout->addWidget(_ratingBar);
  rightLayout->addWidget(new Separator(_rightPanel));

  _developerField = new InfoField("开发者", _rightPanel);
  _categoriesField = new InfoField("类别", _rightPanel);
  rightLayout->addWidget(_developerField);
  rightLayout->addWidget(_categoriesField);

  rightLayout->addWidget(new CaptionLabel("标签", _rightPanel));
  _tagsFlow = new TagFlow(_rightPanel);
  rightLayout->addWidget(_tagsFlow);

  rightLayout->addWidget(new CaptionLabel("相关链接", _rightPanel));
  _linksText = new ElaText(_rightPanel);
  _linksText->setTextPixelSize(13);
  _linksText->setMinimumWidth(0);
  rightLayout->addWidget(_linksText);

  rightLayout->addWidget(new Separator(_rightPanel));
  rightLayout->addWidget(new CaptionLabel("游玩状态", _rightPanel));
  _playStatusBadge = new PlayStatusBadge(_rightPanel);
  rightLayout->addWidget(_playStatusBadge, 0, Qt::AlignLeft);
  rightLayout->addStretch();

  _columnsGrid = new QGridLayout();
  _columnsGrid->setContentsMargins(0, 0, 0, 0);
  _columnsGrid->setHorizontalSpacing(24);
  _columnsGrid->setVerticalSpacing(16);
  _columnsGrid->addWidget(_leftPanel, 0, 0, Qt::AlignTop);
  _columnsGrid->addWidget(_rightPanel, 0, 1, Qt::AlignTop);
  _columnsGrid->setColumnStretch(0, 3);
  _columnsGrid->setColumnStretch(1, 1);
  rootLayout->addLayout(_columnsGrid);
  rootLayout->addStretch();

  addCentralWidget(central);

  _scrollArea = findChild<QScrollArea *>();
  _topButton = new FloatingTopButton(this);
  _topButton->hide();
  connect(_topButton, &FloatingTopButton::clicked, this,
          [this]() { scrollToTopSmooth(); });
  if (_scrollArea) {
    connect(_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int) { updateTopButton(); });
  }

  for (ElaText *text : central->findChildren<ElaText *>()) {
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    text->setFocusPolicy(Qt::ClickFocus);
    text->setContextMenuPolicy(Qt::PreventContextMenu);
  }

  _linksText->setTextFormat(Qt::RichText);
  _linksText->setOpenExternalLinks(true);
  _linksText->setTextInteractionFlags(Qt::TextBrowserInteraction);
  connect(eTheme, &ElaTheme::themeModeChanged, this,
          [this](ElaThemeType::ThemeMode) { updateLinks(); });

  _bannerBackground->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(_bannerBackground, &QWidget::customContextMenuRequested, this,
          &GameDetailPage::showBannerMenu);
  _coverLabel->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(_coverLabel, &QWidget::customContextMenuRequested, this,
          [this](const QPoint &) { showCoverMenu(); });

  setGame(GameInfo{});
}

GameDetailPage::~GameDetailPage() {}

bool GameDetailPage::hasGame() const { return !_game.id.isEmpty(); }

QString GameDetailPage::currentGameId() const { return _game.id; }

void GameDetailPage::setGame(const GameInfo &game) {
  _game = game;
  updateBanner();
  updateCover();
  updateTexts();
  updateLinks();
  _descriptionBrowser->setHtmlContent(game.description);
}

void GameDetailPage::scrollToTop() {
  auto reset = [this]() {
    for (QScrollArea *area : findChildren<QScrollArea *>()) {
      QScrollBar *bar = area->verticalScrollBar();
      bar->setValue(bar->minimum());
    }
  };
  reset();
  // 页面刚切换时滚动区域可能还没完成布局，下一轮事件循环再兜底一次
  QTimer::singleShot(0, this, reset);
}

// 悬浮按钮贴在右下角
void GameDetailPage::positionTopButton() {
  if (!_topButton) {
    return;
  }
  constexpr int kMargin = 24;
  _topButton->move(width() - _topButton->width() - kMargin,
                   height() - _topButton->height() - kMargin);
  _topButton->raise();
}

// 向下滚动一定距离才显示，回到顶部后隐藏
void GameDetailPage::updateTopButton() {
  if (!_topButton || !_scrollArea) {
    return;
  }
  const bool visible = _scrollArea->verticalScrollBar()->value() > 120;
  if (_topButton->isVisible() != visible) {
    _topButton->setVisible(visible);
  }
  if (visible) {
    _topButton->raise();
  }
}

// 平滑滚动到顶部；滚动区不可用时就退化为立即复位
void GameDetailPage::scrollToTopSmooth() {
  if (!_scrollArea) {
    scrollToTop();
    return;
  }
  animateScrollToTop(_scrollArea->verticalScrollBar(), this);
}

void GameDetailPage::resizeEvent(QResizeEvent *event) {
  ElaScrollPage::resizeEvent(event);
  positionTopButton();
  // 窗口被拖到另一块不同缩放的屏幕时，封面要按新的 dpr 重建
  if (_coverLabel &&
      !qFuzzyCompare(_coverLabel->devicePixelRatioF(), _coverDpr)) {
    updateCover();
  }
  if (!_columnsGrid) {
    return;
  }

  constexpr int kCompactWidth = 820;
  const bool compact = width() < kCompactWidth;
  if (compact == _compact) {
    return;
  }
  _compact = compact;

  _columnsGrid->removeWidget(_leftPanel);
  _columnsGrid->removeWidget(_rightPanel);
  if (compact) {
    _columnsGrid->addWidget(_rightPanel, 0, 0, Qt::AlignTop);
    _columnsGrid->addWidget(_leftPanel, 1, 0, Qt::AlignTop);
    _columnsGrid->setColumnStretch(0, 0);
    _columnsGrid->setColumnStretch(1, 0);
  } else {
    _columnsGrid->addWidget(_leftPanel, 0, 0, Qt::AlignTop);
    _columnsGrid->addWidget(_rightPanel, 0, 1, Qt::AlignTop);
    _columnsGrid->setColumnStretch(0, 3);
    _columnsGrid->setColumnStretch(1, 1);
  }
}

void GameDetailPage::updateBanner() {
  QImage image;
  if (!_game.banner.isEmpty()) {
    image.load(QString("%1/%2").arg(bannerDir(), _game.banner));
  }
  if (image.isNull()) {
    image.load(defaultBannerResource());
  }
  if (image.isNull()) {
    image = QImage(1, 1, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
  }
  _bannerBackground->setImage(image);
}

void GameDetailPage::updateCover() {
  const QImage image = currentCoverImage();
  if (image.isNull()) {
    _coverLabel->clear();
    return;
  }
  // 按标签所在屏幕的缩放比生成物理像素：QLabel 绘制带 dpr 的 pixmap
  // 是 1:1 拷贝，不会退化成最近邻放大
  _coverDpr = qMax<qreal>(_coverLabel->devicePixelRatioF(), 1.0);
  _coverLabel->setPixmap(pixmapForDevice(image, _coverLabel->size(),
                                         Qt::KeepAspectRatio, _coverDpr));
}

QImage GameDetailPage::currentCoverImage() const {
  QImage image;
  if (!_game.cover.isEmpty() && _game.cover != defaultCoverKey()) {
    image.load(QString("%1/%2").arg(coverDir(), _game.cover));
  }
  if (image.isNull()) {
    image.load(defaultCoverResource());
  }
  return image;
}

void GameDetailPage::showBannerMenu(const QPoint &pos) {
  if (!_bannerBackground) {
    return;
  }
  const int displayHeight = _bannerBackground->imageDisplayHeight();
  if (displayHeight > 0 && pos.y() > displayHeight) {
    return;
  }
  showImageMenu(_bannerBackground->image(), true);
}

void GameDetailPage::showCoverMenu() {
  showImageMenu(currentCoverImage(), false);
}

void GameDetailPage::showImageMenu(const QImage &image, bool banner) {
  ElaMenu menu(this);
  QAction *viewAction = menu.addAction("查看图片");
  QAction *editAction = menu.addAction("修改");
  QAction *selected = menu.exec(QCursor::pos());
  if (selected == viewAction) {
    if (image.isNull()) {
      showWarning("没有可查看的图片");
      return;
    }
    ImageViewerDialog viewer(image, this);
    viewer.exec();
  } else if (selected == editAction) {
    editImage(banner);
  }
}

void GameDetailPage::editImage(bool banner) {
  if (_game.id.isEmpty()) {
    showWarning("请先选择一个游戏");
    return;
  }
  if (_game.title.trimmed().isEmpty()) {
    showWarning("游戏名称不能为空，无法修改图片");
    return;
  }

  const QImage current =
      banner ? _bannerBackground->image() : currentCoverImage();
  ImageEditDialog dialog(current, _game.title, this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const QImage image = dialog.image();
  if (image.isNull()) {
    return;
  }

  const QString dir = banner ? bannerDir() : coverDir();
  QDir().mkpath(dir);
  const QString fileName =
      QUuid::createUuid().toString(QUuid::WithoutBraces) + ".jpg";
  const QString path = QString("%1/%2").arg(dir, fileName);
  if (!image.save(path, "JPG")) {
    showWarning("保存图片失败");
    return;
  }

  GameInfo game = _game;
  if (banner) {
    game.banner = fileName;
  } else {
    game.cover = fileName;
  }
  if (!DatabaseManager::getInstance()->updateGame(game)) {
    QFile::remove(path);
    showWarning("更新游戏信息失败");
    return;
  }

  const QString oldFile = banner ? _game.banner : _game.cover;
  _game = game;
  if (banner) {
    updateBanner();
  } else {
    updateCover();
  }
  if (!oldFile.isEmpty() && oldFile != defaultCoverKey()) {
    QFile::remove(QString("%1/%2").arg(dir, oldFile));
  }
}

void GameDetailPage::updateTexts() {
  const QString title = _game.title.trimmed();
  _titleText->setText(title.isEmpty() ? QStringLiteral("未命名游戏") : title);
  _ratingBar->setRating(_game.rating);
  _developerField->setValue(_game.developer);
  _categoriesField->setValue(_game.categories);
  _tagsFlow->setTags(splitTags(_game.tags));

  const QString status = _game.playStatus.trimmed();
  _playStatusBadge->setStatus(status.isEmpty() ? defaultPlayStatus() : status);
}

void GameDetailPage::updateLinks() {
  const QStringList lines =
      _game.links.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  const QString linkColor =
      ElaThemeColor(eTheme->getThemeMode(), PrimaryNormal).name();
  QStringList links;
  for (const QString &line : lines) {
    const QString url = line.trimmed();
    if (url.isEmpty()) {
      continue;
    }
    links.append(QString("<a href=\"%1\" style=\"color:%2;"
                         "text-decoration:none;\">%1</a>")
                     .arg(url.toHtmlEscaped(), linkColor));
  }

  _linksText->setText(links.isEmpty() ? QStringLiteral("暂无")
                                      : links.join(QStringLiteral("<br>")));
}

void GameDetailPage::editGameInfo() {
  if (_game.id.isEmpty()) {
    showWarning("请先选择一个游戏");
    return;
  }

  EditGameDialog dialog(_game, this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const GameInfo updated = dialog.updatedGame();
  if (!hasEditableChanges(_game, updated)) {
    return;
  }
  if (!DatabaseManager::getInstance()->updateGame(updated)) {
    showWarning("更新游戏信息失败");
    return;
  }

  _game = updated;
  updateTexts();
  updateLinks();
  _descriptionBrowser->setHtmlContent(updated.description);
  emit gameUpdated(updated.id);
}

} // namespace GManager
