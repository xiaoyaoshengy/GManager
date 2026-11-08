#include "vndb_scraper.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

#include "global_defines.h"
#include "logs.h"

namespace GManager {

namespace {

const QString kApiUrl = QStringLiteral("https://api.vndb.org/kana/vn");
const QString kUserAgent = userAgent();
const QString kVndbBaseUrl = QStringLiteral("https://vndb.org");
const int kSearchResults = 100;
const int kMaxTags = 20;

void logMessage(LogsManager::Level level, const QString &message) {
  LogsManager::getInstance()->logMessage(level, "VndbScraper", message);
}

QString originalLanguageTitle(const QJsonObject &vn) {
  const QString olang = vn.value(QStringLiteral("olang")).toString();
  const QJsonArray titles = vn.value(QStringLiteral("titles")).toArray();

  QString fallback;
  for (const QJsonValue &value : titles) {
    const QJsonObject title = value.toObject();
    if (title.value(QStringLiteral("lang")).toString() != olang) {
      continue;
    }
    const QString text = title.value(QStringLiteral("title")).toString();
    if (text.isEmpty()) {
      continue;
    }
    if (title.value(QStringLiteral("official")).toBool()) {
      return text;
    }
    if (fallback.isEmpty()) {
      fallback = text;
    }
  }

  if (!fallback.isEmpty()) {
    return fallback;
  }

  const QString alttitle = vn.value(QStringLiteral("alttitle")).toString();
  if (!alttitle.isEmpty()) {
    return alttitle;
  }
  return vn.value(QStringLiteral("title")).toString();
}

QString failureMessage(QNetworkReply *reply, const QByteArray &body) {
  if (reply->error() == QNetworkReply::TimeoutError ||
      reply->error() == QNetworkReply::OperationCanceledError) {
    return QStringLiteral("VNDB 请求超时，请检查网络后重试");
  }

  const int status =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  const QString text = QString::fromUtf8(body).trimmed();

  if (status == 429) {
    return QStringLiteral("VNDB 请求过于频繁（HTTP 429），请稍后重试");
  }
  if (text.contains(QStringLiteral("Too much data selected"),
                    Qt::CaseInsensitive)) {
    return QStringLiteral("VNDB 请求数据量过大（Too much data selected）");
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

QString escapeHtml(QString text) {
  text.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
  text.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
  text.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
  return text;
}

QString bbcodeToHtml(const QString &bbcode) {
  QString html = escapeHtml(bbcode);

  static const QRegularExpression urlWithLabel(
      QStringLiteral(R"(\[url=([^\]]+)\](.*?)\[/url\])"),
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::DotMatchesEverythingOption);
  html.replace(urlWithLabel, QStringLiteral(R"(<a href="\1">\2</a>)"));

  static const QRegularExpression urlPlain(
      QStringLiteral(R"(\[url\](.*?)\[/url\])"),
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::DotMatchesEverythingOption);
  html.replace(urlPlain, QStringLiteral(R"(<a href="\1">\1</a>)"));

  static const QRegularExpression image(
      QStringLiteral(R"(\[img\](.*?)\[/img\])"),
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::DotMatchesEverythingOption);
  html.replace(image, QStringLiteral(R"(<img src="\1">)"));

  html.replace(QStringLiteral("[b]"), QStringLiteral("<b>"));
  html.replace(QStringLiteral("[/b]"), QStringLiteral("</b>"));
  html.replace(QStringLiteral("[i]"), QStringLiteral("<i>"));
  html.replace(QStringLiteral("[/i]"), QStringLiteral("</i>"));
  html.replace(QStringLiteral("[u]"), QStringLiteral("<u>"));
  html.replace(QStringLiteral("[/u]"), QStringLiteral("</u>"));
  html.replace(QStringLiteral("[s]"), QStringLiteral("<s>"));
  html.replace(QStringLiteral("[/s]"), QStringLiteral("</s>"));
  html.replace(QStringLiteral("[code]"), QStringLiteral("<pre>"));
  html.replace(QStringLiteral("[/code]"), QStringLiteral("</pre>"));
  html.replace(QStringLiteral("[quote]"), QStringLiteral("<blockquote>"));
  html.replace(QStringLiteral("[/quote]"), QStringLiteral("</blockquote>"));
  html.replace(QStringLiteral("[spoiler]"), QString());
  html.replace(QStringLiteral("[/spoiler]"), QString());
  html.replace(QLatin1Char('\n'), QStringLiteral("<br>"));

  return html;
}

} // namespace

VndbScraper::VndbScraper(QObject *parent) : Scraper(parent) {
  logMessage(LogsManager::Info, "VndbScraper 开始加载");
  _network = new QNetworkAccessManager(this);
}

VndbScraper::~VndbScraper() {}

QString VndbScraper::sourceName() const { return QStringLiteral("vndb"); }

RatingRange VndbScraper::ratingRange() const {
  // VNDB 的 rating 是 Bayesian rating，官方范围 10~100，无人评分时为 null
  return RatingRange{10.0, 100.0};
}

void VndbScraper::fail(const QString &message) {
  logMessage(LogsManager::Warning, QString("请求失败：%1").arg(message));
  emit failed(message);
}

void VndbScraper::search(const QString &query) {
  const QString keyword = query.trimmed();
  if (keyword.isEmpty()) {
    fail(QStringLiteral("搜索关键词不能为空"));
    return;
  }

  logMessage(LogsManager::Info, QString("开始搜索：%1").arg(keyword));

  QJsonArray filters;
  filters.append(QStringLiteral("search"));
  filters.append(QStringLiteral("="));
  filters.append(keyword);
  QJsonObject body;
  body.insert(QStringLiteral("filters"), filters);
  body.insert(QStringLiteral("fields"),
              QStringLiteral("title, alttitle, olang, image.url, "
                             "titles{lang,title,official}"));
  body.insert(QStringLiteral("sort"), QStringLiteral("searchrank"));
  body.insert(QStringLiteral("results"), kSearchResults);

  QNetworkRequest request{QUrl(kApiUrl)};
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/json"));
  request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  request.setTransferTimeout(10000);
#endif

  QNetworkReply *reply = _network->post(
      request, QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply, keyword]() {
    reply->deleteLater();
    const QByteArray payload = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      fail(failureMessage(reply, payload));
      return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
      fail(QStringLiteral("VNDB 返回的不是有效的 JSON 对象"));
      return;
    }

    const QJsonArray items =
        document.object().value(QStringLiteral("results")).toArray();
    QList<ScrapeResult> results;
    for (const QJsonValue &value : items) {
      const QJsonObject vn = value.toObject();
      ScrapeResult result;
      result.id = vn.value(QStringLiteral("id")).toString();
      result.source = sourceName();
      result.title = originalLanguageTitle(vn);
      if (result.title.isEmpty()) {
        continue;
      }
      result.coverUrl = vn.value(QStringLiteral("image"))
                            .toObject()
                            .value(QStringLiteral("url"))
                            .toString();
      results.append(result);
    }

    if (results.isEmpty()) {
      fail(QStringLiteral("VNDB 未找到匹配项"));
      return;
    }

    logMessage(LogsManager::Info, QString("搜索成功：%1（结果：%2 条）")
                                      .arg(keyword)
                                      .arg(results.size()));
    emit searchFinished(results);
  });
}

void VndbScraper::fetchDetails(const QString &id) {
  const QString vnId = id.trimmed();
  if (vnId.isEmpty()) {
    fail(QStringLiteral("结果 ID 为空"));
    return;
  }

  logMessage(LogsManager::Info, QString("开始获取详情：%1").arg(vnId));

  QJsonArray filters;
  filters.append(QStringLiteral("id"));
  filters.append(QStringLiteral("="));
  filters.append(vnId);
  QJsonObject body;
  body.insert(QStringLiteral("filters"), filters);
  body.insert(
      QStringLiteral("fields"),
      QStringLiteral("title, alttitle, olang, image.url, "
                     "titles{lang,title,official}, description, rating, "
                     "developers{name}, tags{name,rating}, "
                     "extlinks{url,label}, screenshots{url}"));
  body.insert(QStringLiteral("results"), 1);

  QNetworkRequest request{QUrl(kApiUrl)};
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/json"));
  request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  request.setTransferTimeout(10000);
#endif

  QNetworkReply *reply = _network->post(
      request, QJsonDocument(body).toJson(QJsonDocument::Compact));
  connect(reply, &QNetworkReply::finished, this, [this, reply, vnId]() {
    reply->deleteLater();
    const QByteArray payload = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      fail(failureMessage(reply, payload));
      return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
      fail(QStringLiteral("VNDB 返回的不是有效的 JSON 对象"));
      return;
    }

    const QJsonArray items =
        document.object().value(QStringLiteral("results")).toArray();
    if (items.isEmpty()) {
      fail(QStringLiteral("VNDB 未找到结果"));
      return;
    }

    const QJsonObject vn = items.first().toObject();
    ScrapeResult result;
    result.id = vnId;
    result.source = sourceName();
    result.title = originalLanguageTitle(vn);
    result.coverUrl = vn.value(QStringLiteral("image"))
                          .toObject()
                          .value(QStringLiteral("url"))
                          .toString();
    result.description =
        bbcodeToHtml(vn.value(QStringLiteral("description")).toString());
    result.rating =
        normalizeRating(vn.value(QStringLiteral("rating")), ratingRange());
    result.categories = QStringLiteral("ADV");

    QStringList developers;
    for (const QJsonValue &value :
         vn.value(QStringLiteral("developers")).toArray()) {
      const QString name =
          value.toObject().value(QStringLiteral("name")).toString();
      if (!name.isEmpty() && !developers.contains(name)) {
        developers.append(name);
      }
    }
    result.developer = developers.join(QStringLiteral("、"));

    QStringList tags;
    for (const QJsonValue &value : vn.value(QStringLiteral("tags")).toArray()) {
      const QJsonObject tag = value.toObject();
      if (tag.value(QStringLiteral("rating")).toDouble() < 1.0) {
        continue;
      }
      const QString name = tag.value(QStringLiteral("name")).toString();
      if (!name.isEmpty() && !tags.contains(name) && tags.size() < kMaxTags) {
        tags.append(name);
      }
    }
    result.tags = tags.join(QStringLiteral("、"));

    QStringList links;
    links.append(QString("%1/%2").arg(kVndbBaseUrl, vnId));
    for (const QJsonValue &value :
         vn.value(QStringLiteral("extlinks")).toArray()) {
      const QString url =
          value.toObject().value(QStringLiteral("url")).toString();
      if (!url.isEmpty() && !links.contains(url)) {
        links.append(url);
      }
    }
    result.links = links.join(QLatin1Char('\n'));

    for (const QJsonValue &value :
         vn.value(QStringLiteral("screenshots")).toArray()) {
      const QJsonObject screenshot = value.toObject();
      const QString url = screenshot.value(QStringLiteral("url")).toString();
      if (url.isEmpty()) {
        continue;
      }
      result.bannerUrls.append(url);
    }

    logMessage(LogsManager::Info, QString("详情获取成功：%1（背景图 %2 张）")
                                      .arg(vnId)
                                      .arg(result.bannerUrls.size()));
    emit detailsReady(result);
  });
}

} // namespace GManager
