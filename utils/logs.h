#ifndef LOGS_H
#define LOGS_H

#include <QObject>

#include "singleton_macro.h"

class QFile;

namespace GManager {

class LogsManager : public QObject {
  Q_OBJECT

public:
  enum Level { Debug, Info, Warning, Error };

  SINGLETON_HEADER(LogsManager)

public:
  void logMessage(LogsManager::Level level, const QString &src,
                  const QString &msg);

private:
  QFile *_logFile{nullptr};
};

} // namespace GManager

#endif // !LOGS_H
