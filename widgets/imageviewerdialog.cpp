#include "imageviewerdialog.h"

#include <QGuiApplication>
#include <QLabel>
#include <QPixmap>
#include <QScreen>

namespace GManager {

ImageViewerDialog::ImageViewerDialog(const QImage &image, QWidget *parent)
    : ElaDialog(parent) {
  setWindowTitle("查看图片");
  setWindowButtonFlag(ElaAppBarType::StayTopButtonHint, false);
  setWindowButtonFlag(ElaAppBarType::MinimizeButtonHint, false);
  setWindowButtonFlag(ElaAppBarType::MaximizeButtonHint, false);
  setIsFixedSize(true);

  const int appBarHeight = getAppBarHeight();
  QSize content = image.size();
  QSize limit(1280, 720 - appBarHeight);
  const QScreen *targetScreen = parent && parent->screen()
                                    ? parent->screen()
                                    : QGuiApplication::primaryScreen();
  if (targetScreen) {
    const QRect available = targetScreen->availableGeometry();
    const QSize screenLimit(qMax(1, available.width() - 80),
                            qMax(1, available.height() - appBarHeight - 80));
    limit = limit.boundedTo(screenLimit);
  }
  if (limit.width() > 0 && limit.height() > 0 &&
      (content.width() > limit.width() || content.height() > limit.height())) {
    content = content.scaled(limit, Qt::KeepAspectRatio);
  }
  if (content.isEmpty()) {
    content = QSize(320, 180);
  }

  const int width = qMax(content.width(), 240);
  setFixedSize(width, content.height() + appBarHeight);

  QLabel *label = new QLabel(this);
  label->setGeometry(0, appBarHeight, width, content.height());
  label->setAlignment(Qt::AlignCenter);
  if (!image.isNull()) {
    label->setPixmap(QPixmap::fromImage(
        image.scaled(content, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
  }
}

ImageViewerDialog::~ImageViewerDialog() {}

} // namespace GManager
