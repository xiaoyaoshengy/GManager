#ifndef IMAGERENDER_H
#define IMAGERENDER_H

#include <QImage>
#include <QPixmap>
#include <QSize>

namespace GManager {

// 逻辑尺寸换算成物理像素尺寸：高分屏下按 devicePixelRatio 放大，至少 1 像素
inline QSize deviceSize(const QSize &logical, qreal devicePixelRatio) {
  const qreal ratio = qMax<qreal>(devicePixelRatio, 1.0);
  return QSize(qMax(1, qRound(logical.width() * ratio)),
               qMax(1, qRound(logical.height() * ratio)));
}

// 平滑缩放到“逻辑尺寸对应的物理像素”，并把结果标记为带 dpr 的图。
// 这样按逻辑坐标绘制时是 1:1 拷贝：不放大就退化成最近邻，也就不会出现锯齿
inline QImage scaleForDevice(const QImage &source, const QSize &logical,
                             Qt::AspectRatioMode mode, qreal devicePixelRatio) {
  const qreal ratio = qMax<qreal>(devicePixelRatio, 1.0);
  QImage scaled =
      source.scaled(deviceSize(logical, ratio), mode, Qt::SmoothTransformation);
  scaled.setDevicePixelRatio(ratio);
  return scaled;
}

inline QPixmap pixmapForDevice(const QImage &source, const QSize &logical,
                               Qt::AspectRatioMode mode,
                               qreal devicePixelRatio) {
  const qreal ratio = qMax<qreal>(devicePixelRatio, 1.0);
  QPixmap pixmap =
      QPixmap::fromImage(scaleForDevice(source, logical, mode, ratio));
  pixmap.setDevicePixelRatio(ratio);
  return pixmap;
}

} // namespace GManager

#endif // !IMAGERENDER_H
