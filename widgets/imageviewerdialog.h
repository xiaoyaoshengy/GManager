#ifndef IMAGEVIEWERDIALOG_H
#define IMAGEVIEWERDIALOG_H

#include <QImage>

#include "ElaDialog.h"

namespace GManager {

// 图片查看器：按原始分辨率显示，窗口尺寸受屏幕大小限制
class ImageViewerDialog : public ElaDialog {
  Q_OBJECT

public:
  explicit ImageViewerDialog(const QImage &image, QWidget *parent = nullptr);
  ~ImageViewerDialog() override;
};

} // namespace GManager

#endif // !IMAGEVIEWERDIALOG_H
