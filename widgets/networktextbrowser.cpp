#include "networktextbrowser.h"

#include <algorithm>

#include <QAbstractScrollArea>
#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPalette>
#include <QResizeEvent>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextImageFormat>
#include <QWheelEvent>
#include <QtMath>

#include "ElaTheme.h"

#include "global_defines.h"
#include "imagecache.h"
#include "logs.h"

namespace GManager {

namespace {

const QString kUserAgent = QStringLiteral("GManager/0.1");

// 介绍正文的默认排版：字号与段落间距，避免原样输出显得拥挤
const QString kHtmlStyleSheet = QStringLiteral(
    "body { font-size: 13px; }"
    "p { margin-top: 4px; margin-bottom: 8px; }"
    "h1 { font-size: 17px; margin-top: 12px; margin-bottom: 6px; }"
    "h2 { font-size: 15px; margin-top: 10px; margin-bottom: 6px; }"
    "h3 { font-size: 14px; margin-top: 8px; margin-bottom: 4px; }"
    "ul, ol { margin-top: 4px; margin-bottom: 8px; }"
    "li { margin-bottom: 3px; }"
    "blockquote { margin-left: 12px; }");

// 段落行高（百分比），让长文本更易读
const int kLineHeightPercent = 150;

void logWarning(const QString &message) {
  LogsManager::getInstance()->logMessage(LogsManager::Warning,
                                         "NetworkTextBrowser", message);
}

} // namespace

NetworkTextBrowser::NetworkTextBrowser(QWidget *parent) : QTextBrowser(parent) {
  _network = new QNetworkAccessManager(this);
  _themeMode = eTheme->getThemeMode();
  setOpenExternalLinks(true);
  setOpenLinks(true);
  setFrameShape(QFrame::NoFrame);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setStyleSheet("QTextBrowser{background:transparent;border:none;}");
  // 介绍区不提供右键菜单，复制仍可走 Ctrl+C
  setContextMenuPolicy(Qt::NoContextMenu);
  document()->setDocumentMargin(0);
  document()->setDefaultStyleSheet(kHtmlStyleSheet);

  connect(document()->documentLayout(),
          &QAbstractTextDocumentLayout::documentSizeChanged, this,
          [this](const QSizeF &) { updateHeight(); });
  connect(eTheme, &ElaTheme::themeModeChanged, this,
          [this](ElaThemeType::ThemeMode themeMode) {
            _themeMode = themeMode;
            applyTheme();
          });

  applyTheme();
  cleanupImageCache();
}

NetworkTextBrowser::~NetworkTextBrowser() {}

void NetworkTextBrowser::setHtmlContent(const QString &html) {
  _html =
      html.trimmed().isEmpty() ? QStringLiteral("<p>暂无详细介绍</p>") : html;
  _images.clear();
  _pending.clear();
  _failed.clear();
  ++_generation;
  reloadHtml();
}

void NetworkTextBrowser::reloadHtml() {
  _imageMaxWidth = -1;
  setHtml(_html);
  isolateImages();
  applyTypography();
  applyImageSizing();
  updateHeight();
}

// 让图片独占一个块：Qt 会把紧跟块级元素的 <img> 并入前一个块，使标题等
// 文本所在行被撑到图片高度、上下出现大片空白。这里按需在图片前后插入块分隔。
void NetworkTextBrowser::isolateImages() {
  QTextDocument *doc = document();
  QVector<int> breaks;
  for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
    const int blockStart = block.position();
    const QString blockText = block.text();
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
      const QTextFragment fragment = it.fragment();
      if (!fragment.charFormat().isImageFormat()) {
        continue;
      }
      const int localPos = fragment.position() - blockStart;
      const int imageLength = fragment.length();
      if (!blockText.left(localPos).trimmed().isEmpty()) {
        breaks.append(fragment.position());
      }
      if (!blockText.mid(localPos + imageLength).trimmed().isEmpty()) {
        breaks.append(fragment.position() + imageLength);
      }
    }
  }
  if (breaks.isEmpty()) {
    return;
  }
  std::sort(breaks.begin(), breaks.end());
  breaks.erase(std::unique(breaks.begin(), breaks.end()), breaks.end());
  QTextCursor cursor(doc);
  cursor.beginEditBlock();
  for (int i = breaks.size() - 1; i >= 0; --i) {
    cursor.setPosition(breaks.at(i));
    cursor.insertBlock();
  }
  cursor.endEditBlock();
}

// 逐段设置行高：QTextDocument 不支持 line-height，只能改块格式。
// 含图片的块跳过：按比例行高会把整行（含图片）一起放大，图片上下留下大片空白
void NetworkTextBrowser::applyTypography() {
  QTextDocument *doc = document();
  QTextCursor cursor(doc);
  cursor.beginEditBlock();
  QTextBlock block = doc->begin();
  int guard = 0;
  while (block.isValid() && ++guard < 20000) {
    bool hasImage = false;
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
      if (it.fragment().charFormat().isImageFormat()) {
        hasImage = true;
        break;
      }
    }
    if (hasImage) {
      block = block.next();
      continue;
    }
    QTextBlockFormat format = block.blockFormat();
    format.setLineHeight(kLineHeightPercent,
                         QTextBlockFormat::ProportionalHeight);
    QTextCursor blockCursor(block);
    blockCursor.setBlockFormat(format);
    block = blockCursor.block().next();
  }
  cursor.endEditBlock();
}

// 图片按可用宽度等比缩放，独占一段时居中显示
void NetworkTextBrowser::applyImageSizing() {
  QTextDocument *doc = document();
  const int maxWidth = qMax(viewport()->width() - 2, 80);
  if (_imageMaxWidth == maxWidth) {
    return;
  }
  _imageMaxWidth = maxWidth;

  struct ImageEdit {
    int position;
    int length;
    QTextImageFormat format;
    int blockPosition;
    bool centerBlock;
  };
  QVector<ImageEdit> edits;

  for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
      const QTextFragment fragment = it.fragment();
      const QTextCharFormat charFormat = fragment.charFormat();
      if (!charFormat.isImageFormat()) {
        continue;
      }
      const QTextImageFormat imageFormat = charFormat.toImageFormat();
      const QImage image =
          doc->resource(QTextDocument::ImageResource, QUrl(imageFormat.name()))
              .value<QImage>();
      if (image.isNull() || image.width() <= 0) {
        continue; // 图片还没加载完成，等载入后 reloadHtml 再处理
      }

      ImageEdit edit;
      edit.position = fragment.position();
      edit.length = fragment.length();
      edit.format = imageFormat;
      const int targetWidth = qMin(image.width(), maxWidth);
      const int targetHeight =
          qMax(1, qRound(qreal(image.height()) * targetWidth / image.width()));
      edit.format.setWidth(targetWidth);
      edit.format.setHeight(targetHeight);
      // 图片不是链接，清除锚点，禁止点击跳转（HTML 层可能仍包在 <a> 里）
      edit.format.setAnchor(false);
      edit.blockPosition = block.position();
      // 图片在 block 里以占位字符（U+FFFC）存在，去掉后为空才算独占一段
      QString blockText = block.text();
      blockText.remove(QChar::ObjectReplacementCharacter);
      edit.centerBlock = blockText.trimmed().isEmpty();
      edits.append(edit);
    }
  }

  if (edits.isEmpty()) {
    return;
  }

  QTextCursor cursor(doc);
  cursor.beginEditBlock();
  for (const ImageEdit &edit : edits) {
    QTextCursor fragmentCursor(doc);
    fragmentCursor.setPosition(edit.position);
    fragmentCursor.setPosition(edit.position + edit.length,
                               QTextCursor::KeepAnchor);
    fragmentCursor.setCharFormat(edit.format);
  }
  for (const ImageEdit &edit : edits) {
    if (!edit.centerBlock) {
      continue;
    }
    const QTextBlock block = doc->findBlock(edit.blockPosition);
    if (!block.isValid()) {
      continue;
    }
    QTextBlockFormat blockFormat = block.blockFormat();
    if (blockFormat.alignment() != Qt::AlignHCenter) {
      blockFormat.setAlignment(Qt::AlignHCenter);
      QTextCursor blockCursor(block);
      blockCursor.setBlockFormat(blockFormat);
    }
  }
  cursor.endEditBlock();
}

QVariant NetworkTextBrowser::loadResource(int type, const QUrl &name) {
  if (type != QTextDocument::ImageResource) {
    return QTextBrowser::loadResource(type, name);
  }

  QUrl url = name;
  if (url.scheme().isEmpty() && name.toString().startsWith("//")) {
    url = QUrl(QStringLiteral("https:") + name.toString());
  }
  const QString scheme = url.scheme().toLower();
  if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
    return QTextBrowser::loadResource(type, name);
  }

  if (_images.contains(url)) {
    return _images.value(url);
  }
  if (_failed.contains(url)) {
    return QImage();
  }

  const QString cachePath = imageCachePath(url);
  QImage image;
  if (QFile::exists(cachePath) && image.load(cachePath)) {
    _images.insert(url, image);
    touchImageCache(cachePath);
    return image;
  }

  if (!_pending.contains(url)) {
    _pending.insert(url);
    requestImage(url);
  }
  return QImage();
}

void NetworkTextBrowser::requestImage(const QUrl &url) {
  const int generation = _generation;

  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  request.setTransferTimeout(15000);
#endif

  QNetworkReply *reply = _network->get(request);
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, url, generation]() {
            reply->deleteLater();
            _pending.remove(url);
            if (generation != _generation) {
              return;
            }

            if (reply->error() != QNetworkReply::NoError) {
              _failed.insert(url);
              logWarning(QString("图片加载失败：%1（%2）")
                             .arg(url.toString(), reply->errorString()));
              return;
            }

            QImage image;
            if (!image.loadFromData(reply->readAll())) {
              _failed.insert(url);
              logWarning(QString("图片数据解析失败：%1").arg(url.toString()));
              return;
            }

            _images.insert(url, image);
            QDir().mkpath(imageCacheDir());
            if (image.save(imageCachePath(url), "PNG")) {
              cleanupImageCache();
            }

            reloadHtml();
          });
}

void NetworkTextBrowser::resizeEvent(QResizeEvent *event) {
  QTextBrowser::resizeEvent(event);
  applyImageSizing();
  updateHeight();
}

// 自身不响应滚轮，把滚轮事件转发给外层滚动区（详情页），保持整页滚动一致
void NetworkTextBrowser::wheelEvent(QWheelEvent *event) {
  for (QWidget *widget = parentWidget(); widget;
       widget = widget->parentWidget()) {
    auto *scrollArea = qobject_cast<QAbstractScrollArea *>(widget);
    if (!scrollArea) {
      continue;
    }
    QCoreApplication::sendEvent(scrollArea->viewport(), event);
    event->accept();
    return;
  }
  event->ignore();
}

void NetworkTextBrowser::updateHeight() {
  const int documentHeight = qCeil(document()->size().height());
  const int target = qMax(documentHeight + frameWidth() * 2, 1);
  if (height() != target) {
    setFixedHeight(target);
  }
}

void NetworkTextBrowser::applyTheme() {
  QPalette palette = this->palette();
  const QColor textColor = ElaThemeColor(_themeMode, BasicText);
  const QColor linkColor = ElaThemeColor(_themeMode, PrimaryNormal);
  palette.setColor(QPalette::Text, textColor);
  palette.setColor(QPalette::WindowText, textColor);
  palette.setColor(QPalette::Link, linkColor);
  palette.setColor(QPalette::LinkVisited, linkColor);
  setPalette(palette);
}

} // namespace GManager
