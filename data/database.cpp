#include "database.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

#include <QUuid>

#include "ElaMessageBar.h"

#include "global_defines.h"
#include "logs.h"
#include "playstatus.h"

namespace GManager {

namespace {

const QString kConnectionName = QStringLiteral("GManagerGames");

const QStringList kGameColumns{"id",          "title",  "cover",      "banner",
                               "description", "rating", "developer",  "tags",
                               "categories",  "links",  "play_status"};

const QString kGameSelect =
    QStringLiteral("SELECT id, title, cover, banner, description, rating, "
                   "developer, tags, categories, links, play_status "
                   "FROM games");

const QString kCreateGamesTable =
    QStringLiteral("CREATE TABLE IF NOT EXISTS games ("
                   "id TEXT PRIMARY KEY NOT NULL,"
                   "title TEXT NOT NULL,"
                   "cover TEXT NOT NULL DEFAULT '',"
                   "banner TEXT NOT NULL DEFAULT '',"
                   "description TEXT NOT NULL DEFAULT '',"
                   "rating TEXT NOT NULL DEFAULT '',"
                   "developer TEXT NOT NULL DEFAULT '',"
                   "tags TEXT NOT NULL DEFAULT '',"
                   "categories TEXT NOT NULL DEFAULT '',"
                   "links TEXT NOT NULL DEFAULT '',"
                   "play_status TEXT NOT NULL DEFAULT '')");

// 空 QString 是 null，直接绑定会被当成 SQL NULL 而违反 NOT NULL 约束
QString toBinding(const QString &value) {
  return value.isNull() ? QStringLiteral("") : value;
}

GameInfo gameFromQuery(const QSqlQuery &query) {
  GameInfo game;
  game.id = query.value(0).toString();
  game.title = query.value(1).toString();
  game.cover = query.value(2).toString();
  game.banner = query.value(3).toString();
  game.description = query.value(4).toString();
  game.rating = query.value(5).toString();
  game.developer = query.value(6).toString();
  game.tags = query.value(7).toString();
  game.categories = query.value(8).toString();
  game.links = query.value(9).toString();
  game.playStatus = query.value(10).toString();
  if (game.playStatus.isEmpty()) {
    game.playStatus = defaultPlayStatus();
  }
  return game;
}

void notifyInputWarning(const QString &message) {
  ElaMessageBar::warning(ElaMessageBarType::TopRight, "警告", message, 2000);
  LogsManager::getInstance()->logMessage(LogsManager::Warning,
                                         "DatabaseManager", message);
}

// 只允许删除 coverDir()/bannerDir() 下的纯文件名，跳过空值与 default
bool removeGameFile(const QString &dir, const QString &fileName) {
  if (fileName.isEmpty() || fileName == defaultCoverKey()) {
    return false;
  }
  if (QFileInfo(fileName).fileName() != fileName) {
    return false;
  }

  const QString path = QDir(dir).filePath(fileName);
  if (!QFile::exists(path)) {
    return false;
  }
  if (!QFile::remove(path)) {
    LogsManager::getInstance()->logMessage(
        LogsManager::Warning, "DatabaseManager",
        QString("删除游戏文件失败：%1").arg(path));
    return false;
  }
  return true;
}

// LIKE 里 % 与 _ 是通配符，关键词必须转义后才能当字面量匹配
QString escapeLikePattern(const QString &text) {
  QString escaped = text;
  escaped.replace('\\', "\\\\");
  escaped.replace('%', "\\%");
  escaped.replace('_', "\\_");
  return escaped;
}

// 拼出分页查询的 WHERE 子句并把绑定值追加到 binds；无条件时返回空串
QString buildFilterClause(const GameFilter &filter, QVariantList *binds) {
  QStringList conditions;
  if (!filter.playStatus.isEmpty()) {
    conditions.append(QStringLiteral("play_status = ?"));
    binds->append(filter.playStatus);
  }

  const QString keyword = filter.keyword.trimmed();
  if (!keyword.isEmpty()) {
    const QString pattern = "%" + escapeLikePattern(keyword) + "%";
    conditions.append(QStringLiteral("(title LIKE ? ESCAPE '\\' OR developer "
                                     "LIKE ? ESCAPE '\\' OR tags LIKE ? ESCAPE "
                                     "'\\')"));
    binds->append(pattern);
    binds->append(pattern);
    binds->append(pattern);
  }

  if (conditions.isEmpty()) {
    return QString();
  }
  return QStringLiteral(" WHERE ") + conditions.join(QStringLiteral(" AND "));
}

} // namespace

SINGLETON_SOURCE(DatabaseManager)

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent) {
  _ready = openDatabase() && ensureSchema();
}

DatabaseManager::~DatabaseManager() {
  if (_db.isOpen()) {
    _db.close();
  }
}

bool DatabaseManager::isReady() const { return _ready; }

QString DatabaseManager::lastError() const { return _lastError; }

QList<GameSummary> DatabaseManager::querySummaries(const GameFilter &filter,
                                                   int offset, int limit,
                                                   bool *hasMore) const {
  QList<GameSummary> summaries;
  if (hasMore) {
    *hasMore = false;
  }
  if (!_ready || limit <= 0) {
    return summaries;
  }

  QVariantList binds;
  const QString where = buildFilterClause(filter, &binds);

  QSqlQuery query(_db);
  // 多取一条用于判断后面还有没有数据，省掉一次 COUNT 查询
  query.prepare(
      QStringLiteral("SELECT id, title, cover, play_status FROM games") +
      where + QStringLiteral(" ORDER BY rowid LIMIT ? OFFSET ?"));
  for (const QVariant &bind : binds) {
    query.addBindValue(bind);
  }
  query.addBindValue(limit + 1);
  query.addBindValue(qMax(0, offset));
  if (!query.exec()) {
    setError("查询游戏列表失败", query.lastError());
    return summaries;
  }

  while (query.next()) {
    if (summaries.size() >= limit) {
      if (hasMore) {
        *hasMore = true;
      }
      break;
    }
    GameSummary summary;
    summary.id = query.value(0).toString();
    summary.title = query.value(1).toString();
    summary.cover = query.value(2).toString();
    summary.playStatus = query.value(3).toString();
    if (summary.playStatus.isEmpty()) {
      summary.playStatus = defaultPlayStatus();
    }
    summaries.append(summary);
  }
  return summaries;
}

bool DatabaseManager::queryRandomId(QString *id) const {
  if (!_ready || !id) {
    return false;
  }

  QSqlQuery query(_db);
  if (!query.exec("SELECT id FROM games ORDER BY RANDOM() LIMIT 1")) {
    setError("查询随机游戏失败", query.lastError());
    return false;
  }
  if (!query.next()) {
    return false;
  }
  *id = query.value(0).toString();
  return true;
}

bool DatabaseManager::queryById(const QString &id, GameInfo *game) const {
  if (!_ready || !game || id.isEmpty()) {
    return false;
  }

  QSqlQuery query(_db);
  query.prepare(kGameSelect + " WHERE id = ?");
  query.addBindValue(id);
  if (!query.exec()) {
    setError("查询游戏失败", query.lastError());
    return false;
  }

  if (!query.next()) {
    return false;
  }

  *game = gameFromQuery(query);
  return true;
}

bool DatabaseManager::insertGame(GameInfo *game) {
  if (!_ready || !game) {
    return false;
  }

  if (!game->id.isEmpty()) {
    GameInfo existing;
    if (queryById(game->id, &existing)) {
      notifyInputWarning(QString("游戏 ID 已存在：%1").arg(game->id));
      return false;
    }
  }

  if (game->title.isEmpty()) {
    notifyInputWarning("游戏名称不能为空");
    return false;
  }

  if (game->id.isEmpty()) {
    game->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  }

  if (game->cover.isEmpty()) {
    game->cover = defaultCoverKey();
  }

  const QString playStatus = game->playStatus.trimmed();
  if (playStatus.isEmpty()) {
    game->playStatus = defaultPlayStatus();
  } else if (!isValidPlayStatus(playStatus)) {
    notifyInputWarning(QString("游玩状态无效：%1").arg(playStatus));
    return false;
  } else {
    game->playStatus = playStatus;
  }

  QSqlQuery query(_db);
  query.prepare(
      "INSERT INTO games(id, title, cover, banner, description, rating, "
      "developer, tags, categories, links, play_status) "
      "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
  query.addBindValue(toBinding(game->id));
  query.addBindValue(toBinding(game->title));
  query.addBindValue(toBinding(game->cover));
  query.addBindValue(toBinding(game->banner));
  query.addBindValue(toBinding(game->description));
  query.addBindValue(toBinding(game->rating));
  query.addBindValue(toBinding(game->developer));
  query.addBindValue(toBinding(game->tags));
  query.addBindValue(toBinding(game->categories));
  query.addBindValue(toBinding(game->links));
  query.addBindValue(toBinding(game->playStatus));
  if (!query.exec()) {
    setError("新增游戏失败", query.lastError());
    return false;
  }
  return true;
}

bool DatabaseManager::insertGames(QList<GameInfo> *games) {
  if (!_ready || !games || games->isEmpty()) {
    return false;
  }

  if (!_db.transaction()) {
    setError("批量新增游戏失败", _db.lastError());
    return false;
  }

  for (GameInfo &game : *games) {
    if (!insertGame(&game)) {
      _db.rollback();
      return false;
    }
  }

  if (!_db.commit()) {
    setError("批量新增游戏失败", _db.lastError());
    _db.rollback();
    return false;
  }
  return true;
}

bool DatabaseManager::updateGame(const GameInfo &game) {
  if (!_ready || game.id.isEmpty()) {
    return false;
  }

  if (game.title.isEmpty()) {
    notifyInputWarning("游戏名称不能为空");
    return false;
  }

  QString playStatus = game.playStatus.trimmed();
  if (playStatus.isEmpty()) {
    playStatus = defaultPlayStatus();
  } else if (!isValidPlayStatus(playStatus)) {
    notifyInputWarning(QString("游玩状态无效：%1").arg(playStatus));
    return false;
  }

  QSqlQuery query(_db);
  query.prepare(
      "UPDATE games SET title = ?, cover = ?, banner = ?, description = ?, "
      "rating = ?, developer = ?, tags = ?, categories = ?, links = ?, "
      "play_status = ? WHERE id = ?");
  query.addBindValue(toBinding(game.title));
  query.addBindValue(toBinding(game.cover));
  query.addBindValue(toBinding(game.banner));
  query.addBindValue(toBinding(game.description));
  query.addBindValue(toBinding(game.rating));
  query.addBindValue(toBinding(game.developer));
  query.addBindValue(toBinding(game.tags));
  query.addBindValue(toBinding(game.categories));
  query.addBindValue(toBinding(game.links));
  query.addBindValue(toBinding(playStatus));
  query.addBindValue(toBinding(game.id));
  if (!query.exec()) {
    setError("更新游戏失败", query.lastError());
    return false;
  }

  if (query.numRowsAffected() == 0) {
    setError("更新游戏失败", QString("记录不存在：%1").arg(game.id));
    return false;
  }
  return true;
}

bool DatabaseManager::updatePlayStatus(const QString &id,
                                       const QString &status) {
  if (!_ready || id.isEmpty()) {
    return false;
  }
  if (!isValidPlayStatus(status)) {
    notifyInputWarning(QString("游玩状态无效：%1").arg(status));
    return false;
  }

  QSqlQuery query(_db);
  query.prepare("UPDATE games SET play_status = ? WHERE id = ?");
  query.addBindValue(toBinding(status));
  query.addBindValue(toBinding(id));
  if (!query.exec()) {
    setError("更新游玩状态失败", query.lastError());
    return false;
  }

  if (query.numRowsAffected() == 0) {
    setError("更新游玩状态失败", QString("记录不存在：%1").arg(id));
    return false;
  }
  return true;
}

bool DatabaseManager::removeGame(const QString &id) {
  if (!_ready || id.isEmpty()) {
    return false;
  }

  GameInfo game;
  if (!queryById(id, &game)) {
    setError("删除游戏失败", QString("记录不存在：%1").arg(id));
    return false;
  }

  QSqlQuery query(_db);
  query.prepare("DELETE FROM games WHERE id = ?");
  query.addBindValue(id);
  if (!query.exec()) {
    setError("删除游戏失败", query.lastError());
    return false;
  }

  if (query.numRowsAffected() == 0) {
    setError("删除游戏失败", QString("记录不存在：%1").arg(id));
    return false;
  }

  const bool coverRemoved = removeGameFile(coverDir(), game.cover);
  const bool bannerRemoved = removeGameFile(bannerDir(), game.banner);
  LogsManager::getInstance()->logMessage(
      LogsManager::Info, "DatabaseManager",
      QString("已删除游戏：%1（封面：%2，横幅：%3）")
          .arg(game.title,
               coverRemoved ? QStringLiteral("已删除") : QStringLiteral("无"),
               bannerRemoved ? QStringLiteral("已删除")
                             : QStringLiteral("无")));
  return true;
}

bool DatabaseManager::clearAll() {
  if (!_ready) {
    return false;
  }

  QSqlQuery query(_db);
  if (!query.exec("DELETE FROM games")) {
    setError("清空游戏失败", query.lastError());
    return false;
  }
  return true;
}

bool DatabaseManager::openDatabase() {
  if (!QDir().mkpath(appDataDir())) {
    setError("打开数据库失败",
             QString("无法创建数据目录：%1").arg(appDataDir()));
    return false;
  }
  QDir().mkpath(coverDir());
  QDir().mkpath(bannerDir());
  QDir().mkpath(imageCacheDir());

  _db = QSqlDatabase::contains(kConnectionName)
            ? QSqlDatabase::database(kConnectionName)
            : QSqlDatabase::addDatabase("QSQLITE", kConnectionName);
  _db.setDatabaseName(databasePath());
  if (!_db.open()) {
    setError("打开数据库失败", _db.lastError());
    return false;
  }
  return true;
}

bool DatabaseManager::ensureSchema() {
  QSqlQuery query(_db);

  if (_db.tables().contains("games")) {
    QStringList columns;
    if (!query.exec("PRAGMA table_info(games)")) {
      setError("读取数据表结构失败", query.lastError());
      return false;
    }
    while (query.next()) {
      columns.append(query.value(1).toString());
    }

    // 旧版本使用 (id int, name text, cover text)，主键语义无法原地迁移
    const bool legacy = columns.contains("name") && !columns.contains("title");
    const bool missingCore =
        !columns.contains("id") || !columns.contains("title");
    if (legacy || missingCore) {
      const QString backupPath =
          QString("%1.bak-%2")
              .arg(databasePath(),
                   QDateTime::currentDateTime().toString("yyyyMMddhhmmss"));
      if (!QFile::copy(databasePath(), backupPath)) {
        setError("升级数据表失败",
                 QString("备份旧数据库失败：%1").arg(backupPath));
        return false;
      }
      if (!query.exec("DROP TABLE games")) {
        setError("升级数据表失败", query.lastError());
        return false;
      }
      LogsManager::getInstance()->logMessage(
          LogsManager::Info, "DatabaseManager",
          QString("旧数据表已备份到 %1 并重建").arg(backupPath));
    } else {
      for (const QString &column : kGameColumns) {
        if (columns.contains(column)) {
          continue;
        }
        const QString sql = QStringLiteral("ALTER TABLE games ADD COLUMN %1 "
                                           "TEXT NOT NULL DEFAULT ''")
                                .arg(column);
        if (!query.exec(sql)) {
          setError("升级数据表失败", query.lastError());
          return false;
        }
      }
    }
  }

  if (!query.exec(kCreateGamesTable)) {
    setError("创建数据表失败", query.lastError());
    return false;
  }
  return true;
}

void DatabaseManager::setError(const QString &context,
                               const QSqlError &error) const {
  setError(context, error.text());
}

void DatabaseManager::setError(const QString &context,
                               const QString &message) const {
  _lastError = QString("%1：%2").arg(context, message);
  LogsManager::getInstance()->logMessage(LogsManager::Error, "DatabaseManager",
                                         _lastError);
}

} // namespace GManager
