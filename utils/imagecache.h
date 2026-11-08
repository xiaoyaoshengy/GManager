#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QString>

class QUrl;

namespace GManager {

// 网络图片磁盘缓存的路径、访问时间与容量清理
QString imageCachePath(const QUrl &url);
void touchImageCache(const QString &path);
void cleanupImageCache();

} // namespace GManager

#endif // !IMAGECACHE_H
