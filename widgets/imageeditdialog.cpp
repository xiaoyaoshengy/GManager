#include "imageeditdialog.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QListView>
#include <QModelIndex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QVBoxLayout>

#include "ElaLineEdit.h"
#include "ElaListView.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaToolButton.h"

#include "scraper.h"
#include "scrapesearchdialog.h"

namespace GManager {

namespace {

const QString kUserAgent = QStringLiteral("GManager/0.1");
const int kMessageDuration = 2000;
// 缩略图块高度，实际按候选条视口高度复算，保证竖向铺满
const int kCandidateItemHeight = 88;
const int kThumbnailAspectWidth = 16;
const int kThumbnailAspectHeight = 9;
// 相邻缩略图之间的水平间隔（竖向不留白）
const int kThumbnailGap = 6;

void showWarning(const QString &message) {
  ElaMessageBar::warning(ElaMessageBarType::TopRight, "警告", message,
                         kMessageDuration);
}

// 按格子裁切铺满；选中时在图片内侧画高亮边框
class CandidateDelegate : public QStyledItemDelegate {
public:
  explicit CandidateDelegate(QListView *view, QObject *parent = nullptr)
      : QStyledItemDelegate(parent), _view(view) {}

  // 必须按格子尺寸返回，否则 IconMode 会用「图标 + 文本」的默认高度
  // 分配 item 矩形，格子虽高却只在顶部画出一条缩略图
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    Q_UNUSED(option);
    Q_UNUSED(index);
    const QSize grid = _view->gridSize();
    return grid.isValid() && !grid.isEmpty() ? grid : _view->iconSize();
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 竖向铺满整格，只在左右留出间隔，否则缩略图下方会出现一条留白
    const QRect content =
        option.rect.adjusted(kThumbnailGap / 2, 0, -kThumbnailGap / 2, 0);
    const QPixmap pixmap = index.data(Qt::DecorationRole).value<QPixmap>();
    if (!pixmap.isNull()) {
      const QPixmap scaled =
          pixmap.scaled(content.size(), Qt::KeepAspectRatioByExpanding,
                        Qt::SmoothTransformation);
      const QRect source((scaled.width() - content.width()) / 2,
                         (scaled.height() - content.height()) / 2,
                         content.width(), content.height());
      painter->drawPixmap(content, scaled, source);
    } else {
      painter->fillRect(content, option.palette.base());
    }

    if (option.state & QStyle::State_Selected) {
      painter->setBrush(Qt::NoBrush);
      painter->setPen(QPen(option.palette.highlight().color(), 3));
      painter->drawRect(content.adjusted(2, 2, -2, -2));
    }
    painter->restore();
  }

private:
  QListView *_view{nullptr};
};

} // namespace

ImageEditDialog::ImageEditDialog(const QImage &current,
                                 const QString &scrapeTitle, QWidget *parent)
    : ElaDialog(parent), _scrapeTitle(scrapeTitle.trimmed()),
      _current(current) {
  setWindowTitle("修改图片");
  setFixedSize(760, 740);
  setWindowButtonFlag(ElaAppBarType::StayTopButtonHint, false);
  setWindowButtonFlag(ElaAppBarType::MinimizeButtonHint, false);
  setWindowButtonFlag(ElaAppBarType::MaximizeButtonHint, false);
  setIsFixedSize(true);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(20, getAppBarHeight() + 8, 20, 20);
  mainLayout->setSpacing(10);

  ElaText *titleText =
      new ElaText("输入图片链接、从本地选择，或从刮削器获取", this);
  titleText->setTextPixelSize(14);

  QHBoxLayout *inputLayout = new QHBoxLayout();
  _urlEdit = new ElaLineEdit(this);
  _urlEdit->setPlaceholderText("图片链接");
  _loadButton = new ElaPushButton("加载", this);
  _fileButton = new ElaPushButton("选择文件", this);
  _loadButton->setFixedHeight(35);
  _fileButton->setFixedHeight(35);
  inputLayout->addWidget(_urlEdit, 1);
  inputLayout->addWidget(_loadButton);
  inputLayout->addWidget(_fileButton);

  QHBoxLayout *scrapeLayout = new QHBoxLayout();
  _scrapeButton = new ElaToolButton(this);
  _scrapeButton->setText("刮削图片");
  _scrapeButton->setElaIcon(ElaIconType::Images);
  _scrapeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  _scrapeButton->setIsTransparent(false);
  _scrapeButton->setFixedHeight(35);
  _scrapeButton->setEnabled(!_scrapeTitle.isEmpty());
  _scrapeButton->setToolTip(
      _scrapeTitle.isEmpty()
          ? QStringLiteral("需要游戏名称才能搜索")
          : QStringLiteral("按游戏名称搜索刮削源并挑选图片"));
  _scrapeStatus = new ElaText(this);
  _scrapeStatus->setTextPixelSize(12);
  _scrapeStatus->setText(
      _scrapeTitle.isEmpty()
          ? QStringLiteral("缺少游戏名称，无法搜索刮削器")
          : QStringLiteral("可按游戏名称搜索刮削源的封面与背景图"));
  scrapeLayout->addWidget(_scrapeButton);
  scrapeLayout->addWidget(_scrapeStatus, 1);

  _preview = new QLabel(this);
  _preview->setFixedSize(700, 280);
  _preview->setAlignment(Qt::AlignCenter);

  _candidateView = new ElaListView(this);
  _candidateModel = new QStandardItemModel(this);
  _candidateView->setModel(_candidateModel);
  _candidateView->setViewMode(QListView::IconMode);
  _candidateView->setFlow(QListView::LeftToRight);
  _candidateView->setWrapping(false);
  _candidateView->setResizeMode(QListView::Adjust);
  _candidateView->setMovement(QListView::Static);
  _candidateView->setSpacing(0);
  // 强制常显水平滚动条，让视口高度恒定，缩略图能精确铺满整个高度
  _candidateView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  const int scrollBarExtent = _candidateView->style()->pixelMetric(
      QStyle::PM_ScrollBarExtent, nullptr,
      _candidateView->horizontalScrollBar());
  _candidateView->setFixedHeight(kCandidateItemHeight + scrollBarExtent +
                                 2 * _candidateView->frameWidth());
  _candidateView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  _candidateView->setSelectionMode(QAbstractItemView::SingleSelection);
  _candidateView->setItemDelegate(
      new CandidateDelegate(_candidateView, _candidateView));
  // 视口尺寸一旦确定（显示、滚动条出现）就重新贴合副略图
  _candidateView->viewport()->installEventFilter(this);
  updateCandidateGridSize();
  _candidateView->hide();

  _hint = new ElaText(this);
  _hint->setTextPixelSize(12);

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  ElaPushButton *cancelButton = new ElaPushButton("取消", this);
  ElaPushButton *confirmButton = new ElaPushButton("确认", this);
  cancelButton->setFixedHeight(35);
  confirmButton->setFixedHeight(35);
  buttonLayout->addStretch();
  buttonLayout->addWidget(cancelButton);
  buttonLayout->addWidget(confirmButton);

  mainLayout->addWidget(titleText);
  mainLayout->addLayout(inputLayout);
  mainLayout->addLayout(scrapeLayout);
  mainLayout->addWidget(_preview, 0, Qt::AlignHCenter);
  mainLayout->addWidget(_candidateView);
  mainLayout->addWidget(_hint);
  mainLayout->addStretch();
  mainLayout->addLayout(buttonLayout);

  _network = new QNetworkAccessManager(this);

  connect(_loadButton, &QPushButton::clicked, this,
          &ImageEditDialog::loadFromUrl);
  connect(_urlEdit, &QLineEdit::returnPressed, this,
          &ImageEditDialog::loadFromUrl);
  connect(_fileButton, &QPushButton::clicked, this,
          &ImageEditDialog::chooseFile);
  connect(_scrapeButton, &QPushButton::clicked, this,
          &ImageEditDialog::scrapeImages);
  connect(_candidateView, &QListView::clicked, this,
          &ImageEditDialog::handleCandidateClicked);
  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  connect(confirmButton, &QPushButton::clicked, this,
          &ImageEditDialog::confirmSelection);

  if (current.isNull()) {
    _hint->setText("尚未选择图片");
  } else {
    setPreview(current);
  }
}

ImageEditDialog::~ImageEditDialog() {}

QImage ImageEditDialog::image() const { return _image; }

bool ImageEditDialog::eventFilter(QObject *watched, QEvent *event) {
  if (watched == _candidateView->viewport() &&
      event->type() == QEvent::Resize) {
    updateCandidateGridSize();
  }
  return ElaDialog::eventFilter(watched, event);
}

void ImageEditDialog::loadFromUrl() {
  const QString text = _urlEdit->text().trimmed();
  if (text.isEmpty()) {
    showWarning("请输入图片链接");
    return;
  }

  const QUrl target(text);
  if (!target.isValid() || target.scheme().isEmpty()) {
    showWarning("图片链接无效");
    return;
  }

  _hint->setText("正在加载图片…");
  _loadButton->setEnabled(false);

  QNetworkRequest request{target};
  request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  request.setTransferTimeout(15000);
#endif
  QNetworkReply *reply = _network->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    _loadButton->setEnabled(true);

    if (reply->error() != QNetworkReply::NoError) {
      _hint->setText(QString("加载失败：%1").arg(reply->errorString()));
      return;
    }

    QImage image;
    if (!image.loadFromData(reply->readAll())) {
      _hint->setText("图片数据无法解析");
      return;
    }
    setPreview(image);
  });
}

void ImageEditDialog::chooseFile() {
  const QString path = QFileDialog::getOpenFileName(
      this, "选择图片", QString(),
      "图片文件 (*.png *.jpg *.jpeg *.bmp *.webp *.gif)");
  if (path.isEmpty()) {
    return;
  }

  QImage image;
  if (!image.load(path)) {
    showWarning("无法加载选中的图片");
    return;
  }
  _urlEdit->clear();
  setPreview(image);
}

void ImageEditDialog::setPreview(const QImage &image) {
  _image = image;
  _preview->setPixmap(QPixmap::fromImage(image.scaled(
      _preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
  _hint->setText(
      QString("预览：%1 × %2").arg(image.width()).arg(image.height()));
}

void ImageEditDialog::updateCandidateGridSize() {
  // 未显示时视口尺寸还没有意义，先用固定值，等 show 后再按真实视口高度校准
  int itemHeight = kCandidateItemHeight;
  if (_candidateView->isVisible() && _candidateView->viewport()->height() > 0) {
    itemHeight = _candidateView->viewport()->height();
  }
  const int itemWidth =
      qMax(1, itemHeight * kThumbnailAspectWidth / kThumbnailAspectHeight);
  const QSize grid(itemWidth, itemHeight);
  if (_candidateView->gridSize() == grid) {
    return;
  }
  _candidateView->setGridSize(grid);
  _candidateView->setIconSize(grid);
}

QSize ImageEditDialog::thumbnailSize() const {
  // 缩略图按格子的两倍像素准备，避免高分屏下发虚
  const QSize icon = _candidateView->iconSize();
  return QSize(qMax(1, icon.width()) * 2, qMax(1, icon.height()) * 2);
}

void ImageEditDialog::scrapeImages() {
  if (_scrapeTitle.isEmpty()) {
    return;
  }

  ScrapeSearchDialog search(_scrapeTitle, this);
  if (search.exec() != QDialog::Accepted) {
    return;
  }

  const ScrapeResult detail = search.detail();
  QStringList urls;
  auto appendUrl = [&urls](const QString &url) {
    const QString trimmed = url.trimmed();
    if (!trimmed.isEmpty() && !urls.contains(trimmed)) {
      urls.append(trimmed);
    }
  };
  appendUrl(detail.coverUrl);
  for (const QString &url : detail.bannerUrls) {
    appendUrl(url);
  }

  _candidateUrls = urls;
  rebuildCandidates();
}

void ImageEditDialog::rebuildCandidates() {
  ++_generation;
  _candidateModel->clear();
  _cachedImages.clear();
  _pendingPreviewUrl.clear();
  _pending = 0;
  _loaded = 0;

  // 第 0 项固定为当前图片，用于退回原始图片
  QStandardItem *currentItem = new QStandardItem();
  currentItem->setEditable(false);
  if (!_current.isNull()) {
    currentItem->setData(
        QVariant::fromValue(QPixmap::fromImage(_current.scaled(
            thumbnailSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation))),
        Qt::DecorationRole);
    currentItem->setToolTip("当前图片");
  }
  _candidateModel->appendRow(currentItem);

  for (const QString &url : _candidateUrls) {
    QStandardItem *item = new QStandardItem();
    item->setEditable(false);
    item->setToolTip(url);
    _candidateModel->appendRow(item);
  }

  if (_candidateUrls.isEmpty()) {
    // 仅保留“当前图片”时也需要能退回，所以有原图才显示候选条
    _candidateView->setVisible(!_current.isNull());
    _scrapeStatus->setText("该刮削结果没有可用图片");
    loadCandidateThumbnails();
    return;
  }

  _candidateView->show();
  // 显示后视口高度才最终确定，这里再校准一次格子尺寸
  updateCandidateGridSize();
  _candidateView->setCurrentIndex(_candidateModel->index(0, 0));
  loadCandidateThumbnails();
  updateScrapeHint();
}

void ImageEditDialog::loadCandidateThumbnails() {
  for (int i = 0; i < _candidateUrls.size(); ++i) {
    const QString url = _candidateUrls.at(i);
    if (_cachedImages.contains(url)) {
      ++_loaded;
      continue;
    }

    ++_pending;
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(15000);
#endif
    const int generation = _generation;
    QNetworkReply *reply = _network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, url, generation]() {
              reply->deleteLater();
              // 重新刮削后旧回包直接丢弃，否则计数会错乱
              if (generation != _generation) {
                return;
              }
              --_pending;

              QImage image;
              if (reply->error() == QNetworkReply::NoError &&
                  image.loadFromData(reply->readAll())) {
                _cachedImages.insert(url, image);
                ++_loaded;
                const int row = _candidateUrls.indexOf(url) + 1;
                if (row > 0) {
                  QStandardItem *item = _candidateModel->item(row);
                  if (item) {
                    item->setData(
                        QVariant::fromValue(QPixmap::fromImage(
                            image.scaled(thumbnailSize(), Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation))),
                        Qt::DecorationRole);
                    _candidateView->doItemsLayout();
                  }
                }
                // 用户先点了这张图但当时还在下载，这里补上预览
                if (_pendingPreviewUrl == url) {
                  _pendingPreviewUrl.clear();
                  setPreview(image);
                  const int chosen = _candidateUrls.indexOf(url) + 1;
                  _scrapeStatus->setText(
                      QString("当前使用：刮削器图片（第 %1 张）").arg(chosen));
                }
              }
              updateScrapeHint();
            });
  }
  updateScrapeHint();
}

void ImageEditDialog::handleCandidateClicked(const QModelIndex &index) {
  if (!index.isValid()) {
    return;
  }

  const int row = index.row();
  if (row == 0) {
    _pendingPreviewUrl.clear();
    if (_current.isNull()) {
      showWarning("没有可恢复的原始图片");
      return;
    }
    setPreview(_current);
    _scrapeStatus->setText("当前使用：原始图片");
    return;
  }

  const QString url = _candidateUrls.value(row - 1);
  if (url.isEmpty()) {
    return;
  }

  if (_cachedImages.contains(url)) {
    _pendingPreviewUrl.clear();
    setPreview(_cachedImages.value(url));
    _scrapeStatus->setText(
        QString("当前使用：刮削器图片（第 %1 张）").arg(row));
    return;
  }

  _pendingPreviewUrl = url;
  _scrapeStatus->setText("正在加载所选图片…");
}

void ImageEditDialog::updateScrapeHint() {
  if (!_scrapeStatus) {
    return;
  }
  if (_candidateUrls.isEmpty()) {
    return;
  }
  if (_pending > 0) {
    _scrapeStatus->setText(QString("正在加载图片…（%1/%2）")
                               .arg(_loaded)
                               .arg(_candidateUrls.size()));
    return;
  }
  _scrapeStatus->setText(
      QString("共 %1 张，已加载 %2 张；第一个缩略图为当前图片")
          .arg(_candidateUrls.size())
          .arg(_loaded));
}

void ImageEditDialog::confirmSelection() {
  if (_image.isNull()) {
    showWarning("请先加载一张图片");
    return;
  }
  accept();
}

} // namespace GManager
