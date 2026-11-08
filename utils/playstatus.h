#ifndef PLAYSTATUS_H
#define PLAYSTATUS_H

#include <QString>
#include <QStringList>

namespace GManager {

// 允许的游玩状态，顺序即界面展示顺序
inline QStringList playStatusOptions() {
  return {QStringLiteral("未游玩"), QStringLiteral("游玩中"),
          QStringLiteral("已完成"), QStringLiteral("搁置"),
          QStringLiteral("抛弃")};
}

inline QString defaultPlayStatus() { return QStringLiteral("未游玩"); }

inline bool isValidPlayStatus(const QString &status) {
  return playStatusOptions().contains(status);
}

} // namespace GManager

#endif // !PLAYSTATUS_H
