#include "bangumi_scraper.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

#include "appsettings.h"
#include "global_defines.h"
#include "logs.h"

namespace GManager {

namespace {

const QString kBaseUrl = QStringLiteral("https://api.bgm.tv");
const QString kUserAgent = userAgent();
// /v0/search/subjects 的服务端上限是 20，超过会被静默截断
const int kSearchResults = 20;
// 条目类型：4 为游戏
const int kGameSubjectType = 4;
const int kTransferTimeout = 10000;

const QStringList kDeveloperKeys{
    QStringLiteral("开发"), QStringLiteral("游戏开发商"),
    QStringLiteral("开发商"), QStringLiteral("Developer"),
    QStringLiteral("Developers")};
const QStringList kPublisherKeys{
    QStringLiteral("发行"), QStringLiteral("发行商"),
    QStringLiteral("Publisher"), QStringLiteral("Publishers")};
const QStringList kGenreKeys{QStringLiteral("类型"),
                             QStringLiteral("游戏类型")};

void logMessage(LogsManager::Level level, const QString &message) {
  LogsManager::getInstance()->logMessage(level, "BangumiScraper", message);
}

QString escapeHtml(QString text) {
  text.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
  text.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
  text.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
  return text;
}

// 统一的失败提示：区分认证失效、UA 被拒、条目不可见与限流
QString failureMessage(QNetworkReply *reply, const QByteArray &body,
                       bool detailRequest) {
  if (reply->error() == QNetworkReply::TimeoutError ||
      reply->error() == QNetworkReply::OperationCanceledError) {
    return QStringLiteral("Bangumi 请求超时，请检查网络后重试");
  }

  const int status =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  const QString text = QString::fromUtf8(body).trimmed();

  switch (status) {
  case 401:
    return QStringLiteral("Bangumi Token 无效或已过期，请在设置中重新填写");
  case 403:
    return QStringLiteral("Bangumi 拒绝了请求（User-Agent 不合规）：%1")
        .arg(text.left(120));
  case 404:
    if (detailRequest) {
      // 服务端对「无权限查看 R18 条目」与「条目不存在」返回同一个 404
      return QStringLiteral("该条目在 Bangumi 不可见：R18 条目需要有效的 "
                            "Bangumi Token（且账号注册满 60 天），也可能"
                            "是条目已被删除");
    }
    return QStringLiteral("Bangumi 未找到匹配的条目");
  case 429:
    return QStringLiteral(
        "Bangumi 请求过于频繁（已触发限流，约 1 小时后恢复）");
  default:
    break;
  }

  QString message = reply->errorString();
  if (status > 0) {
    message = QString("HTTP %1: %2").arg(status).arg(message);
  }
  if (!text.isEmpty()) {
    message += QString(" - %1").arg(text.left(200));
  }
  return message;
}

// 构造请求：带上合规的 User-Agent，配置了 Token 时附带 Bearer 认证。
// /v0 只接受请求头传 Token，不再支持 query string 形式。
QNetworkRequest makeRequest(const QUrl &url) {
  QNetworkRequest request(url);
  request.setRawHeader("Accept", "application/json");
  request.setRawHeader("User-Agent", kUserAgent.toUtf8());

  const QString token = AppSettings::getInstance()->bangumiToken();
  if (!token.isEmpty()) {
    request.setRawHeader("Authorization",
                         QString("Bearer %1").arg(token).toUtf8());
  }

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  request.setTransferTimeout(kTransferTimeout);
#endif
  return request;
}

// 从 infobox 中按候选键取第一个命中的值
QStringList infoboxValues(const QJsonArray &infobox, const QStringList &keys,
                          const QString &separator = QStringLiteral("、")) {
  for (const QJsonValue &value : infobox) {
    const QJsonObject item = value.toObject();
    if (!keys.contains(item.value(QStringLiteral("key")).toString())) {
      continue;
    }
    const QJsonValue field = item.value(QStringLiteral("value"));
    QStringList out;
    if (field.isString()) {
      const QRegularExpression sep(separator);
      for (const QString &part :
           field.toString().split(sep, Qt::SkipEmptyParts)) {
        out.append(part.trimmed());
      }
    } else if (field.isArray()) {
      for (const QJsonValue &entry : field.toArray()) {
        if (entry.isString()) {
          out.append(entry.toString().trimmed());
        } else if (entry.isObject()) {
          const QJsonObject object = entry.toObject();
          for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.value().isString()) {
              out.append(it.value().toString().trimmed());
              break;
            }
          }
        }
      }
    }
    return out;
  }
  return {};
}

QString subjectTitle(const QJsonObject &subject) {
  const QString cn =
      subject.value(QStringLiteral("name_cn")).toString().trimmed();
  if (!cn.isEmpty()) {
    return cn;
  }
  return subject.value(QStringLiteral("name")).toString().trimmed();
}

// 详情的描述：summary 转义后把换行统一成 <br>
QString summaryToHtml(const QString &summary) {
  QString html = escapeHtml(summary.trimmed());
  html.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  html.replace(QLatin1Char('\r'), QLatin1Char('\n'));
  html.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
  return html;
}

// 标签取 tags[].name，去重保序
QString subjectTags(const QJsonArray &tags) {
  QStringList names;
  for (const QJsonValue &value : tags) {
    const QString name =
        value.toObject().value(QStringLiteral("name")).toString().trimmed();
    if (!name.isEmpty() && !names.contains(name)) {
      names.append(name);
    }
  }
  return names.join(QStringLiteral("、"));
}

// 开发者优先取「开发」类键，缺失时回退「发行」类键
QString subjectDeveloper(const QJsonArray &infobox) {
  QStringList developers = infoboxValues(infobox, kDeveloperKeys);
  if (developers.isEmpty()) {
    developers = infoboxValues(infobox, kPublisherKeys);
  }
  return developers.join(QStringLiteral("、"));
}

} // namespace

BangumiScraper::BangumiScraper(QObject *parent) : Scraper(parent) {
  logMessage(LogsManager::Info, "BangumiScraper 开始加载");
  _network = new QNetworkAccessManager(this);
}

BangumiScraper::~BangumiScraper() {}

QString BangumiScraper::sourceName() const { return QStringLiteral("bangumi"); }

RatingRange BangumiScraper::ratingRange() const {
  // /v0 的 rating.score 为 0~10，无人评分时缺省
  return RatingRange{0.0, 10.0};
}

void BangumiScraper::fail(const QString &message) {
  logMessage(LogsManager::Warning, QString("请求失败：%1").arg(message));
  emit failed(message);
}

void BangumiScraper::search(const QString &query) {
  const QString keyword = query.trimmed();
  if (keyword.isEmpty()) {
    fail(QStringLiteral("搜索关键词不能为空"));
    return;
  }

  logMessage(LogsManager::Info, QString("开始搜索：%1").arg(keyword));

  const QUrl url(QString("%1/v0/search/subjects?limit=%2&offset=0")
                     .arg(kBaseUrl)
                     .arg(kSearchResults));

  QJsonObject filter;
  filter.insert(QStringLiteral("type"), QJsonArray{kGameSubjectType});
  // 刻意不传 filter.nsfw：该字段是过滤语义（true 只要 R18、false 只要非
  // R18），缺省时不限制；无权限的请求会被服务端强制成 false，因此未配置
  // Token 时只会得到全年龄条目，不会出现「搜到但点开 404」。
  QJsonObject body;
  body.insert(QStringLiteral("keyword"), keyword);
  body.insert(QStringLiteral("sort"), QStringLiteral("match"));
  body.insert(QStringLiteral("filter"), filter);

  QNetworkRequest request = makeRequest(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/json"));

  QNetworkReply *reply = _network->post(
      request, QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply, keyword]() {
    reply->deleteLater();
    const QByteArray payload = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      fail(failureMessage(reply, payload, false));
      return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
      fail(QStringLiteral("Bangumi 返回的不是有效的 JSON 对象"));
      return;
    }

    const QJsonArray list =
        document.object().value(QStringLiteral("data")).toArray();
    QList<ScrapeResult> results;
    for (const QJsonValue &value : list) {
      const QJsonObject subject = value.toObject();
      const int id = subject.value(QStringLiteral("id")).toInt();
      const QString title = subjectTitle(subject);
      if (id <= 0 || title.isEmpty()) {
        continue;
      }

      // v0 的搜索结果已是完整 Subject，可直接填充基础信息，
      // 详情请求失败时也能入库较完整的条目
      const QJsonArray infobox =
          subject.value(QStringLiteral("infobox")).toArray();
      ScrapeResult result;
      result.id = QString::number(id);
      result.source = sourceName();
      result.title = title;
      result.coverUrl = subject.value(QStringLiteral("images"))
                            .toObject()
                            .value(QStringLiteral("large"))
                            .toString();
      result.rating = normalizeRating(subject.value(QStringLiteral("rating"))
                                          .toObject()
                                          .value(QStringLiteral("score")),
                                      ratingRange());
      result.developer = subjectDeveloper(infobox);
      result.categories =
          infoboxValues(infobox, kGenreKeys, QStringLiteral(" / "))
              .join(QStringLiteral("、"));
      result.tags =
          subjectTags(subject.value(QStringLiteral("tags")).toArray());
      results.append(result);
    }

    if (results.isEmpty()) {
      fail(QStringLiteral("Bangumi 未找到匹配项"));
      return;
    }

    logMessage(LogsManager::Info, QString("搜索成功：%1（结果：%2 条）")
                                      .arg(keyword)
                                      .arg(results.size()));
    emit searchFinished(results);
  });
}

void BangumiScraper::fetchDetails(const QString &id) {
  const QString subjectId = id.trimmed();
  if (subjectId.isEmpty()) {
    fail(QStringLiteral("结果 ID 为空"));
    return;
  }

  logMessage(LogsManager::Info, QString("开始获取详情：%1").arg(subjectId));

  const QUrl url(QString("%1/v0/subjects/%2").arg(kBaseUrl, subjectId));
  QNetworkReply *reply = _network->get(makeRequest(url));
  connect(reply, &QNetworkReply::finished, this, [this, reply, subjectId]() {
    reply->deleteLater();
    const QByteArray payload = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      fail(failureMessage(reply, payload, true));
      return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
      fail(QStringLiteral("Bangumi 返回的不是有效的 JSON 对象"));
      return;
    }
    const QJsonObject subject = document.object();
    const QJsonArray infobox =
        subject.value(QStringLiteral("infobox")).toArray();

    ScrapeResult result;
    result.id = subjectId;
    result.source = sourceName();
    result.title = subjectTitle(subject);
    result.coverUrl = subject.value(QStringLiteral("images"))
                          .toObject()
                          .value(QStringLiteral("large"))
                          .toString();
    result.description =
        summaryToHtml(subject.value(QStringLiteral("summary")).toString());
    result.rating = normalizeRating(subject.value(QStringLiteral("rating"))
                                        .toObject()
                                        .value(QStringLiteral("score")),
                                    ratingRange());
    result.developer = subjectDeveloper(infobox);
    result.categories =
        infoboxValues(infobox, kGenreKeys, QStringLiteral(" / "))
            .join(QStringLiteral("、"));
    result.tags = subjectTags(subject.value(QStringLiteral("tags")).toArray());

    QStringList links;
    links.append(QString("https://bgm.tv/subject/%1").arg(subjectId));
    for (const QJsonValue &value : infobox) {
      const QJsonObject item = value.toObject();
      const QString key = item.value(QStringLiteral("key")).toString();
      if (key != QStringLiteral("website") && key != QStringLiteral("链接")) {
        continue;
      }
      const QJsonValue field = item.value(QStringLiteral("value"));
      if (field.isString()) {
        links.append(field.toString().trimmed());
      } else if (field.isArray()) {
        for (const QJsonValue &site : field.toArray()) {
          const QJsonObject object = site.toObject();
          QString url = object.value(QStringLiteral("v")).toString();
          if (url.isEmpty()) {
            url = object.value(QStringLiteral("url")).toString();
          }
          if (!url.isEmpty()) {
            links.append(url.trimmed());
          }
        }
      }
    }
    links.removeDuplicates();
    result.links = links.join(QLatin1Char('\n'));

    logMessage(LogsManager::Info, QString("详情获取成功：%1").arg(subjectId));
    emit detailsReady(result);
  });
}

} // namespace GManager
