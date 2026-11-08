#include "appsettings.h"

#include <QDir>
#include <QFile>
#include <QSettings>

#include "global_defines.h"

namespace GManager {

namespace {

// 配置文件内的键名，避免调用处硬编码
const QString kBangumiTokenKey = QStringLiteral("scrapers/bangumi/token");

} // namespace

SINGLETON_SOURCE(AppSettings)

AppSettings::AppSettings(QObject *parent) : QObject(parent) {
  QDir().mkpath(appDataDir());
  _settings = new QSettings(configPath(), QSettings::IniFormat);
  // 配置里有凭据，只允许当前用户读写
  QFile::setPermissions(configPath(), QFile::ReadOwner | QFile::WriteOwner);
}

AppSettings::~AppSettings() {
  delete _settings;
  _settings = nullptr;
}

QString AppSettings::bangumiToken() const {
  return _settings->value(kBangumiTokenKey).toString().trimmed();
}

void AppSettings::setBangumiToken(const QString &token) {
  const QString value = token.trimmed();
  if (value.isEmpty()) {
    _settings->remove(kBangumiTokenKey);
  } else {
    _settings->setValue(kBangumiTokenKey, value);
  }
  // 立即落盘，避免异常退出丢配置
  _settings->sync();
  QFile::setPermissions(configPath(), QFile::ReadOwner | QFile::WriteOwner);
}

bool AppSettings::hasBangumiToken() const { return !bangumiToken().isEmpty(); }

} // namespace GManager
