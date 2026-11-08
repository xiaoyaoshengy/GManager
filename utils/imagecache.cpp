#include "imagecache.h"

#include <algorithm>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QUrl>

#include "global_defines.h"
#include "logs.h"

namespace GManager {

namespace {

constexpr qint64 kMaxCacheBytes = 128LL * 1024 * 1024;
constexpr int kMaxCacheFiles = 500;

bool isCacheFileName(const QString &fileName) {
  if (!fileName.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
    return false;
  }
  const QString stem = fileName.left(fileName.size() - 4);
  if (stem.size() != 40) {
    return false;
  }
  for (const QChar &character : stem) {
    if (!((character >= QLatin1Char('0') && character <= QLatin1Char('9')) ||
          (character >= QLatin1Char('a') && character <= QLatin1Char('f')))) {
      return false;
    }
  }
  return true;
}

QDateTime cacheAccessTime(const QFileInfo &info) {
  const QDateTime lastRead = info.lastRead();
  return lastRead.isValid() ? lastRead : info.lastModified();
}

} // namespace

QString imageCachePath(const QUrl &url) {
  const QByteArray hash =
      QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha1)
          .toHex();
  return QString("%1/%2.png").arg(imageCacheDir(), QString::fromLatin1(hash));
}

void touchImageCache(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadWrite)) {
    return;
  }
  file.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileAccessTime);
  file.close();
}

void cleanupImageCache() {
  QDir dir(imageCacheDir());
  if (!dir.exists()) {
    return;
  }

  QFileInfoList entries =
      dir.entryInfoList(QStringList{QStringLiteral("*.png")}, QDir::Files);
  entries.erase(std::remove_if(entries.begin(), entries.end(),
                               [](const QFileInfo &info) {
                                 return !isCacheFileName(info.fileName());
                               }),
                entries.end());

  std::sort(entries.begin(), entries.end(),
            [](const QFileInfo &left, const QFileInfo &right) {
              return cacheAccessTime(left) < cacheAccessTime(right);
            });

  qint64 totalBytes = 0;
  for (const QFileInfo &info : entries) {
    totalBytes += info.size();
  }

  int removed = 0;
  qint64 freedBytes = 0;
  int index = 0;
  while (index < entries.size() && (entries.size() - index > kMaxCacheFiles ||
                                    totalBytes > kMaxCacheBytes)) {
    const QFileInfo &info = entries.at(index++);
    const qint64 size = info.size();
    if (!QFile::remove(info.absoluteFilePath())) {
      continue;
    }
    ++removed;
    freedBytes += size;
    totalBytes -= size;
  }

  if (removed > 0) {
    LogsManager::getInstance()->logMessage(
        LogsManager::Info, "ImageCache",
        QString("清理图片缓存 %1 个文件，释放 %2 字节")
            .arg(removed)
            .arg(freedBytes));
  }
}

} // namespace GManager
