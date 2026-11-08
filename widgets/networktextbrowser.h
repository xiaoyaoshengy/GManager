#ifndef NETWORKTEXTBROWSER_H
#define NETWORKTEXTBROWSER_H

#include <QHash>
#include <QImage>
#include <QSet>
#include <QTextBrowser>
#include <QUrl>

#include "ElaWidgetToolsDef.h"

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

private:
  void requestImage(const QUrl &url);
  void reloadHtml();
  void isolateImages();
  void applyTypography();
  void applyImageSizing();
  void updateHeight();
  void applyTheme();

  QNetworkAccessManager *_network{nullptr};
  QHash<QUrl, QImage> _images;
  QSet<QUrl> _pending;
  QSet<QUrl> _failed;
  QString _html;
  int _generation{0};
  int _imageMaxWidth{-1};
  ElaThemeType::ThemeMode _themeMode;
};

} // namespace GManager

#endif // !NETWORKTEXTBROWSER_H
