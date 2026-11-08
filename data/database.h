#ifndef DATABASE_H
#define DATABASE_H

#include <QList>
#include <QObject>
#include <QSqlDatabase>
#include <QString>

#include "singleton_macro.h"

class QSqlError;

namespace GManager {

struct GameInfo {
  QString id;          // UUID 文本主键
  QString title;       // 游戏名
  QString cover;       // 封面文件名，不含目录
  QString banner;      // 横幅背景图文件名，不含目录
  QString description; // 详细介绍，HTML
  QString rating;      // 评分
  QString developer;   // 开发者
  QString tags;        // 标签
  QString categories;  // 类别
  QString links;       // 相关链接，按行分隔
  QString playStatus;  // 游玩状态
};

// 卡片列表查询条件，空字符串表示该项不过滤
struct GameFilter {
  QString keyword;    // 匹配 title/developer/tags
  QString playStatus; // 精确匹配游玩状态
};

// 卡片列表所需的最小字段集，不含 description 等大字段
struct GameSummary {
  QString id;
  QString title;
  QString cover;
  QString playStatus;
};

class DatabaseManager : public QObject {
  Q_OBJECT

  SINGLETON_HEADER(DatabaseManager)

public:
  // 数据库是否已成功打开并完成建表
  bool isReady() const;
  QString lastError() const;

  // 按插入顺序分页查询卡片摘要；hasMore 表示后面还有数据。
  // 首页靠它避免一次把全部游戏读进内存
  QList<GameSummary> querySummaries(const GameFilter &filter, int offset,
                                    int limit, bool *hasMore = nullptr) const;
  // 随机取一条游戏主键，库为空时返回 false
  bool queryRandomId(QString *id) const;
  bool queryById(const QString &id, GameInfo *game) const;

  // id 为空时自动生成 UUID 并回填
  bool insertGame(GameInfo *game);
  bool insertGames(QList<GameInfo> *games);
  bool updateGame(const GameInfo &game);
  bool updatePlayStatus(const QString &id, const QString &status);
  bool removeGame(const QString &id);
  bool clearAll();

private:
  bool openDatabase();
  bool ensureSchema();
  void setError(const QString &context, const QSqlError &error) const;
  void setError(const QString &context, const QString &message) const;

  QSqlDatabase _db;
  bool _ready{false};
  mutable QString _lastError;
};

} // namespace GManager

#endif // !DATABASE_H
