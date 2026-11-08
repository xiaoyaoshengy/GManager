#ifndef IMAGEEDITDIALOG_H
#define IMAGEEDITDIALOG_H

#include <QHash>
#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>

#include "ElaDialog.h"

class ElaLineEdit;
class ElaListView;
class ElaPushButton;
class ElaText;
class ElaToolButton;
class QEvent;
class QLabel;
class QModelIndex;
class QNetworkAccessManager;
class QStandardItemModel;

namespace GManager {

// 图片修改器：从链接、本地文件或刮削器选择并预览，确认后返回选中的图片
class ImageEditDialog : public ElaDialog {
  Q_OBJECT

public:
  // scrapeTitle 为搜索刮削器用的游戏名称，为空时禁用刮削入口
  explicit ImageEditDialog(const QImage &current, const QString &scrapeTitle,
                           QWidget *parent = nullptr);
  ~ImageEditDialog() override;

  QImage image() const;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void loadFromUrl();
  void chooseFile();
  void setPreview(const QImage &image);
  void confirmSelection();
  void scrapeImages();
  void rebuildCandidates();
  void loadCandidateThumbnails();
  void handleCandidateClicked(const QModelIndex &index);
  void updateScrapeHint();
  // 让缩略图的高度跟随候选条视口，避免下方留白
  void updateCandidateGridSize();
  QSize thumbnailSize() const;

  ElaLineEdit *_urlEdit{nullptr};
  ElaPushButton *_loadButton{nullptr};
  ElaPushButton *_fileButton{nullptr};
  ElaToolButton *_scrapeButton{nullptr};
  QLabel *_preview{nullptr};
  ElaListView *_candidateView{nullptr};
  QStandardItemModel *_candidateModel{nullptr};
  ElaText *_hint{nullptr};
  ElaText *_scrapeStatus{nullptr};
  QNetworkAccessManager *_network{nullptr};
  QString _scrapeTitle;
  QStringList _candidateUrls;
  QHash<QString, QImage> _cachedImages;
  QImage _current;
  QImage _image;
  QString _pendingPreviewUrl;
  int _pending{0};
  int _loaded{0};
  int _generation{0};
};

} // namespace GManager

#endif // !IMAGEEDITDIALOG_H
