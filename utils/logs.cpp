#include "logs.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTextStream>

#include "global_defines.h"

namespace GManager {

namespace {

constexpr int kMaxLogFiles = 5;

void cleanupOldLogs(const QString &directory) {
  QDir dir(directory);
  QStringList logs = dir.entryList(QStringList{QStringLiteral("*.log")},
                                   QDir::Files, QDir::NoSort);
  std::sort(
      logs.begin(), logs.end(),
      [](const QString &left, const QString &right) { return left > right; });
  for (int i = kMaxLogFiles; i < logs.size(); ++i) {
    QFile::remove(dir.filePath(logs.at(i)));
  }
}

} // namespace

SINGLETON_SOURCE(LogsManager)

LogsManager::LogsManager(QObject *parent) : QObject(parent) {
  QDir().mkpath(logDir());
  QString fileName =
      QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss");
  _logFile = new QFile(QString("%1/%2.log").arg(logDir(), fileName));
  QIODevice::OpenMode mode =
      (_logFile->exists() ? QIODevice::ExistingOnly : QIODevice::NewOnly) |
      QIODevice::Text;
  if (!_logFile->open(mode)) {
    qWarning() << "[WARNING][LogsManager] 打开日志文件失败";
    if (_logFile->error() != QFile::NoError) {
      qWarning() << "[WARNING][LogsManager] 报错信息\n"
                 << _logFile->errorString();
    }
  } else {
    cleanupOldLogs(logDir());
  }
}

LogsManager::~LogsManager() {
  _logFile->close();
  delete _logFile;
  _logFile = nullptr;
}

void LogsManager::logMessage(LogsManager::Level level, const QString &src,
                             const QString &msg) {
  mtx.lock();

  QString formattedMsg;
  QString colorCode;
  switch (level) {
  case LogsManager::Debug:
    colorCode = "\033[36m"; // 青色
    formattedMsg = QString("[DEBUG][%1] %2").arg(src).arg(msg);
    break;
  case LogsManager::Info:
    colorCode = "\033[32m"; // 绿色
    formattedMsg = QString("[INFO][%1] %2").arg(src).arg(msg);
    break;
  case LogsManager::Warning:
    colorCode = "\033[33m"; // 黄色
    formattedMsg = QString("[WARNING][%1] %2").arg(src).arg(msg);
    break;
  case LogsManager::Error:
    colorCode = "\033[31m"; // 红色
    formattedMsg = QString("[ERROR][%1] %2").arg(src).arg(msg);
    break;
  }

  // 输出到控制台
  qDebug().noquote() << colorCode << formattedMsg << "\033[0m";

  // 写入到文件
  QTextStream stream(_logFile);
  stream << formattedMsg << "\n";

  mtx.unlock();
}

} // namespace GManager
