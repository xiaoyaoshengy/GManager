#ifndef IMAGEPICKERDIALOG_H
#define IMAGEPICKERDIALOG_H

#include <QStringList>

#include "ElaDialog.h"

class ElaListView;
class ElaText;
class QNetworkAccessManager;
class QStandardItemModel;
class QWheelEvent;

namespace GManager {

// 背景图选择器：按背景图列表选择一张，返回原图 URL
class ImagePickerDialog : public ElaDialog {
  Q_OBJECT

public:
  explicit ImagePickerDialog(const QStringList &imageUrls,
                             QWidget *parent = nullptr);
  ~ImagePickerDialog() override;

  QString selectedUrl() const;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void loadPreviews();
  void updateGridSize();
  void updateHint();
  void confirmSelection();
  bool handleWheel(QWheelEvent *event);

  ElaListView *_view{nullptr};
  QStandardItemModel *_model{nullptr};
  ElaText *_hintText{nullptr};
  QNetworkAccessManager *_network{nullptr};
  QStringList _imageUrls;
  QString _selectedUrl;
  int _pending{0};
  int _loaded{0};
  bool _forwardingWheel{false};
};

} // namespace GManager

#endif // !IMAGEPICKERDIALOG_H
