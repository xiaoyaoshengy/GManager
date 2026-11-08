#ifndef GLOBAL_DEFINES_H
#define GLOBAL_DEFINES_H

#include <QCoreApplication>
#include <QStandardPaths>
#include <QString>

namespace GManager {

// 程序版本号：发布新版本只改这一处。
// CMake 会从这个字面量解析版本号（project VERSION 与打包脚本都用它），
// 网络请求的 User-Agent 也由它拼出，不要在别处再写死版本号
constexpr const char *kAppVersion = "1.0.0";

inline QString appVersion() { return QString::fromLatin1(kAppVersion); }

// 统一的 User-Agent，形如 GManager/1.0.0
inline QString userAgent() {
  return QStringLiteral("GManager/%1").arg(appVersion());
}

// 应用数据根目录：Windows 使用可执行文件目录，Linux 遵循 XDG 规范
inline QString appDataDir() {
#ifdef Q_OS_WIN
  return QCoreApplication::applicationDirPath();
#else
  return QString("%1/%2").arg(
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation),
      QCoreApplication::applicationName());
#endif
}

// 封面根目录，数据库中仅保存文件名
inline QString coverDir() {
#ifdef Q_OS_WIN
  return appDataDir() + "/res/covers";
#else
  return appDataDir() + "/data/covers";
#endif
}

// 数据库文件路径
inline QString databasePath() { return appDataDir() + "/games.db"; }

// 用户配置文件路径（QSettings INI），存放刮削器凭据等本机配置
inline QString configPath() { return appDataDir() + "/config.ini"; }

// 横幅背景图目录，数据库中仅保存文件名
inline QString bannerDir() {
#ifdef Q_OS_WIN
  return appDataDir() + "/res/banners";
#else
  return appDataDir() + "/data/banners";
#endif
}

// 网络图片磁盘缓存目录
inline QString imageCacheDir() { return appDataDir() + "/cache/images"; }

// 日志根目录：Windows 为 exe 目录下的 logs，Linux 为 XDG 数据目录
inline QString logDir() { return appDataDir() + "/logs"; }

// 数据库中表示“使用默认封面”的占位值
inline QString defaultCoverKey() { return QStringLiteral("default"); }

// 默认封面资源路径，与 GameCard 的初始封面保持一致
inline QString defaultCoverResource() {
  return QStringLiteral(":/assets/images/buka.jpg");
}

// 默认横幅资源路径，详情页缺少背景图时使用
inline QString defaultBannerResource() {
  return QStringLiteral(":/assets/images/banner.png");
}

} // namespace GManager

#endif // !GLOBAL_DEFINES_H
