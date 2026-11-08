#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QObject>
#include <QString>

#include "singleton_macro.h"

class QSettings;

namespace GManager {

// 本机用户配置：目前只保存各刮削源的凭据，文件为 appDataDir()/config.ini
class AppSettings : public QObject {
  Q_OBJECT

  SINGLETON_HEADER(AppSettings)

public:
  // Bangumi 个人 Access Token，未配置时返回空字符串
  QString bangumiToken() const;
  // 传入空字符串等同于清空配置
  void setBangumiToken(const QString &token);
  bool hasBangumiToken() const;

private:
  QSettings *_settings{nullptr};
};

} // namespace GManager

#endif // !APPSETTINGS_H
