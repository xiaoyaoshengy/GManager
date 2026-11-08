#include "imagepickerdialog.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QImage>
#include <QListView>
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
#include <QWheelEvent>

#include "ElaListView.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"

namespace GManager {

namespace {

const QString kUserAgent = QStringLiteral("GManager/0.1");
const int kThumbnailGap = 8;
const int kThumbnailAspectWidth = 16;
const int kThumbnailAspectHeight = 9;

// 滚轮灵敏度系数（相对默认减少到一半）
const qreal kScrollSensitivity = 0.5;

// 按格子裁切铺满；选中时在图片内侧画高亮边框
class ThumbnailDelegate : public QStyledItemDelegate {
public:
  explicit ThumbnailDelegate(QListView *view, QObject *parent = nullptr)
      : QStyledItemDelegate(parent), _view(view) {}

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    Q_UNUSED(option);
    Q_UNUSED(index);
    const QSize grid = _view->gridSize();
    return grid.isValid() && !grid.isEmpty() ? grid : QSize(160, 90);
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRect content =
        option.rect.adjusted(kThumbnailGap / 2, kThumbnailGap / 2,
                             -kThumbnailGap / 2, -kThumbnailGap / 2);
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

ImagePickerDialog::ImagePickerDialog(const QStringList &imageUrls,
                                     QWidget *parent)
    : ElaDialog(parent), _imageUrls(imageUrls) {
  setWindowTitle("选择背景图");
  setFixedSize(720, 520);
  setWindowButtonFlag(ElaAppBarType::StayTopButtonHint, false);
  setWindowButtonFlag(ElaAppBarType::MinimizeButtonHint, false);
  setWindowButtonFlag(ElaAppBarType::MaximizeButtonHint, false);
  setIsFixedSize(true);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(20, getAppBarHeight() + 8, 20, 20);
  mainLayout->setSpacing(10);

  ElaText *titleText = new ElaText("请选择一张图片作为背景图", this);
  titleText->setTextPixelSize(14);

  _view = new ElaListView(this);
  _model = new QStandardItemModel(this);
  _view->setModel(_model);
  _view->setViewMode(QListView::IconMode);
  _view->setResizeMode(QListView::Adjust);
  _view->setMovement(QListView::Static);
  _view->setSpacing(0);
  _view->setUniformItemSizes(false);
  _view->setEditTriggers(QAbstractItemView::NoEditTriggers);
  _view->setSelectionMode(QAbstractItemView::SingleSelection);
  _view->setItemDelegate(new ThumbnailDelegate(_view, _view));
  _view->viewport()->installEventFilter(this);
  updateGridSize();

  for (int i = 0; i < _imageUrls.size(); ++i) {
    QStandardItem *item = new QStandardItem();
    item->setEditable(false);
    item->setData(_imageUrls.at(i), Qt::UserRole);
    _model->appendRow(item);
  }
  if (_model->rowCount() > 0) {
    _view->setCurrentIndex(_model->index(0, 0));
  }

  _hintText = new ElaText(this);
  _hintText->setTextPixelSize(12);

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  ElaPushButton *cancelButton = new ElaPushButton("取消", this);
  ElaPushButton *confirmButton = new ElaPushButton("确认", this);
  cancelButton->setFixedHeight(35);
  confirmButton->setFixedHeight(35);
  buttonLayout->addStretch();
  buttonLayout->addWidget(cancelButton);
  buttonLayout->addWidget(confirmButton);

  mainLayout->addWidget(titleText);
  mainLayout->addWidget(_view, 1);
  mainLayout->addWidget(_hintText);
  mainLayout->addLayout(buttonLayout);

  _network = new QNetworkAccessManager(this);

  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  connect(confirmButton, &QPushButton::clicked, this,
          &ImagePickerDialog::confirmSelection);
  connect(_view, &QListView::doubleClicked, this,
          [this](const QModelIndex &index) {
            _view->setCurrentIndex(index);
            confirmSelection();
          });

  loadPreviews();
  updateHint();
}

ImagePickerDialog::~ImagePickerDialog() {}

QString ImagePickerDialog::selectedUrl() const { return _selectedUrl; }

bool ImagePickerDialog::eventFilter(QObject *watched, QEvent *event) {
  if (watched == _view->viewport()) {
    if (event->type() == QEvent::Resize) {
      updateGridSize();
    } else if (event->type() == QEvent::Wheel) {
      return handleWheel(static_cast<QWheelEvent *>(event));
    }
  }
  return ElaDialog::eventFilter(watched, event);
}

// 降低滚轮灵敏度：把事件的角度/像素增量按系数缩小后转发给视图，仍交给
// ElaScrollBar 原本的平滑滚动动画处理（直接改滚动值会丢掉动画效果）
bool ImagePickerDialog::handleWheel(QWheelEvent *event) {
  if (_forwardingWheel) {
    return false;
  }
  _forwardingWheel = true;
  const QPoint pixelDelta(qRound(event->pixelDelta().x() * kScrollSensitivity),
                          qRound(event->pixelDelta().y() * kScrollSensitivity));
  const QPoint angleDelta(qRound(event->angleDelta().x() * kScrollSensitivity),
                          qRound(event->angleDelta().y() * kScrollSensitivity));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPointF pos = event->position();
  const QPointF globalPos = event->globalPosition();
#else
  const QPointF pos = event->posF();
  const QPointF globalPos = event->globalPosF();
#endif
  QWheelEvent scaled(pos, globalPos, pixelDelta, angleDelta, event->buttons(),
                     event->modifiers(), event->phase(), event->inverted());
  QApplication::sendEvent(_view->viewport(), &scaled);
  _forwardingWheel = false;
  return true;
}

void ImagePickerDialog::updateGridSize() {
  const int scrollBarExtent = _view->style()->pixelMetric(
      QStyle::PM_ScrollBarExtent, nullptr, _view->verticalScrollBar());
  const int available = _view->maximumViewportSize().width() - scrollBarExtent;
  if (available <= 0) {
    return;
  }
  const int columnWidth = qMax(1, available / 2 - 1);
  const int columnHeight =
      qMax(1, columnWidth * kThumbnailAspectHeight / kThumbnailAspectWidth);
  _view->setGridSize(QSize(columnWidth, columnHeight));
}

void ImagePickerDialog::loadPreviews() {
  _pending = 0;
  _loaded = 0;

  for (int i = 0; i < _imageUrls.size(); ++i) {
    const QString previewUrl = _imageUrls.at(i);
    if (previewUrl.isEmpty()) {
      continue;
    }

    ++_pending;
    QNetworkRequest request{QUrl(previewUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(15000);
#endif

    QStandardItem *item = _model->item(i);
    QNetworkReply *reply = _network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, item]() {
      reply->deleteLater();
      --_pending;

      QImage image;
      if (reply->error() == QNetworkReply::NoError &&
          image.loadFromData(reply->readAll())) {
        ++_loaded;
        const QPixmap pixmap = QPixmap::fromImage(image);
        item->setData(QVariant::fromValue(pixmap), Qt::DecorationRole);
        _view->doItemsLayout();
      }
      updateHint();
    });
  }
}

void ImagePickerDialog::updateHint() {
  if (_imageUrls.isEmpty()) {
    _hintText->setText("该刮削结果没有可用的背景图");
  } else if (_pending > 0) {
    _hintText->setText(QString("正在加载背景图…（%1/%2）")
                           .arg(_loaded)
                           .arg(_imageUrls.size()));
  } else {
    _hintText->setText(
        QString("共 %1 张，已加载 %2 张").arg(_imageUrls.size()).arg(_loaded));
  }
}

void ImagePickerDialog::confirmSelection() {
  const QModelIndex index = _view->currentIndex();
  if (!index.isValid()) {
    ElaMessageBar::warning(ElaMessageBarType::TopRight, "警告",
                           "请选择一张背景图", 2000);
    return;
  }

  _selectedUrl = index.data(Qt::UserRole).toString();
  if (_selectedUrl.isEmpty()) {
    ElaMessageBar::warning(ElaMessageBarType::TopRight, "警告",
                           "请选择一张背景图", 2000);
    return;
  }
  accept();
}

} // namespace GManager
