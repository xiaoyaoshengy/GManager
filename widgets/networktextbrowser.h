#ifndef NETWORKTEXTBROWSER_H
#define NETWORKTEXTBROWSER_H

#include <QHash>
#include <QImage>
#include <QSet>
#include <QTextBrowser>
#include <QUrl>

#include "ElaWidgetToolsDef.h"

class QEvent;
class QNetworkAccessManager;
class QWheelEvent;

namespace GManager {

// 富文本浏览器：自动异步加载 HTML 中的网络图片，并按文档高度自适应
class NetworkTextBrowser : public QTextBrowser {
  Q_OBJECT

public:
  explicit NetworkTextBrowser(QWidget *parent = nullptr);
  ~NetworkTextBrowser() override;

  void setHtmlContent(const QString &html);

protected:
  QVariant loadResource(int type, const QUrl &name) override;
  void resizeEvent(QResizeEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  bool event(QEvent *event) override;

private:
  void requestImage(const QUrl &url);
  QImage displayImage(const QUrl &url);
  int availableImageWidth() const;
  void reloadHtml();
  void isolateImages();
  void applyTypography();
  void applyImageSizing();
  void updateHeight();
  void applyTheme();

  QNetworkAccessManager *_network{nullptr};
  QHash<QUrl, QImage> _images;        // 原始图片（下载/磁盘缓存得到的全尺寸图）
  QHash<QUrl, QImage> _displayImages; // 按显示尺寸平滑缩放后的图片
  QSet<QUrl> _pending;
  QSet<QUrl> _failed;
  QString _html;
  int _generation{0};
  int _imageMaxWidth{-1};
  qreal _displayDpr{1.0};
  ElaThemeType::ThemeMode _themeMode;
};

} // namespace GManager

#endif // !NETWORKTEXTBROWSER_H
