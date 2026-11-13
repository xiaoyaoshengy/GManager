#include "fanza_scraper.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

#include "logs.h"
#include "scraper_utils.h"

namespace GManager {

namespace {

const QString kBaseUrl = QStringLiteral("https://dlsoft.dmm.co.jp");
const QString kUserAgent = QStringLiteral(
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/140.0.0.0 Safari/537.36");

void logMessage(LogsManager::Level level, const QString &message) {
  LogsManager::getInstance()->logMessage(level, "FanzaScraper", message);
}

QString failureMessage(QNetworkReply *reply) {
  if (reply->error() == QNetworkReply::TimeoutError ||
      reply->error() == QNetworkReply::OperationCanceledError) {
    return QStringLiteral("FANZA 请求超时，请检查网络后重试");
  }
  const int status =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  QString message = reply->errorString();
  if (status > 0) {
    message = QString("HTTP %1: %2").arg(status).arg(message);
  }
  return message;
}

QNetworkRequest makeRequest(const QUrl &url) {
  QNetworkRequest request(url);
  request.setRawHeader("User-Agent", kUserAgent.toUtf8());
  request.setRawHeader("Accept",
                       "text/html,application/xhtml+xml,application/xml;q=0.9,"
                       "image/webp,*/*;q=0.8");
  request.setRawHeader("Referer", "https://www.dmm.co.jp/");
  request.setRawHeader("Cookie", "age_check_done=1; ckcy=1");
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  request.setTransferTimeout(15000);
#endif
  return request;
}

QString classInnerHtml(const QString &html, const QString &className) {
  const QRegularExpression regex(
      QStringLiteral("<[^>]*class=\"[^\"]*%1[^\"]*\"[^>]*>(.*?)"
                     "</(?:div|dd|dt|span|p|ul|h1|h2|h3)\\s*>")
          .arg(className),
      QRegularExpression::CaseInsensitiveOption);
  return scraper_utils::firstCapture(
      html, regex.pattern(), 1,
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::DotMatchesEverythingOption);
}

QList<ScrapeResult> parseSearch(const QString &html, const QString &source) {
  QList<ScrapeResult> results;
  static const QRegularExpression blockRe(
      QStringLiteral("<[a-zA-Z0-9]+[^>]*class=\"[^\"]*productListItem"
                     "[^\"]*\"[^>]*>"),
      QRegularExpression::CaseInsensitiveOption);

  QVector<QRegularExpressionMatch> matches;
  QRegularExpressionMatchIterator it = blockRe.globalMatch(html);
  while (it.hasNext()) {
    matches.append(it.next());
  }

  for (int i = 0; i < matches.size(); ++i) {
    const int start = matches.at(i).capturedStart();
    const int end = (i + 1 < matches.size()) ? matches.at(i + 1).capturedStart()
                                             : html.size();
    const QString block = html.mid(start, end - start);

    QString id = scraper_utils::firstCapture(
        block, QStringLiteral("data-js-pj-add-basket-product-id=\"([^\"]+)\""));
    if (id.isEmpty()) {
      id = scraper_utils::firstCapture(block,
                                       QStringLiteral("detail/([^.\"'/]+)/"));
    }
    if (id.isEmpty()) {
      continue;
    }

    QString name = scraper_utils::plainText(classInnerHtml(
        block, QStringLiteral("component-cardProductBuy__title")));
    if (name.isEmpty()) {
      name = scraper_utils::firstCapture(
          block, QStringLiteral("<a[^>]*title=\"([^\"]+)\""));
    }
    if (name.isEmpty()) {
      continue;
    }

    ScrapeResult result;
    result.id = id;
    result.source = source;
    result.title =
        scraper_utils::collapseWhitespace(scraper_utils::decodeEntities(name));
    results.append(result);
  }
  return results;
}

ScrapeResult parseDetails(const QString &html, const QString &id,
                          const QString &source) {
  ScrapeResult result;
  result.id = id;
  result.source = source;

  result.title = scraper_utils::plainText(
      classInnerHtml(html, QStringLiteral("productTitle__item--headline")));

  const int marker = html.indexOf(QStringLiteral("id=\"detailGuide\""));
  const QString rawDescription = scraper_utils::sanitizeDescriptionHtml(
      scraper_utils::containerInnerHtml(html, marker));
  result.description = scraper_utils::optimizeDescription(rawDescription);

  QStringList links;
  links.append(QString("%1/detail/%2/").arg(kBaseUrl, id));
  result.links = links.join(QLatin1Char('\n'));

  // 背景图：slider 区域中 url 含作品 id 的图片
  for (const QString &raw : scraper_utils::captures(
           html, QStringLiteral("<[^>]*src=\"([^\"]+)\""))) {
    const QString url = scraper_utils::absoluteUrl(raw);
    if (url.contains(id) && url.contains(QStringLiteral(".jpg")) &&
        !result.bannerUrls.contains(url)) {
      result.bannerUrls.append(url);
    }
  }

  // 封面：含 <id>pl 的图片，否则用首张背景图
  for (const QString &url : result.bannerUrls) {
    if (url.contains(id + QStringLiteral("pl"))) {
      result.coverUrl = url;
      break;
    }
  }
  if (result.coverUrl.isEmpty() && !result.bannerUrls.isEmpty()) {
    result.coverUrl = result.bannerUrls.first();
  }

  return result;
}

} // namespace

FanzaScraper::FanzaScraper(QObject *parent) : Scraper(parent) {
  logMessage(LogsManager::Info, "FanzaScraper 开始加载");
  _network = new QNetworkAccessManager(this);
  // 关闭自动 Cookie 罐：否则首个响应的 Set-Cookie 会覆盖手写的 Cookie 头，
  // 后续请求会退回年龄确认页，导致详情解析失败
  _network->setCookieJar(nullptr);
}

FanzaScraper::~FanzaScraper() {}

QString FanzaScraper::sourceName() const { return QStringLiteral("fanza"); }

RatingRange FanzaScraper::ratingRange() const {
  // FANZA 无评分字段，占位
  return RatingRange{0.0, 100.0};
}

void FanzaScraper::fail(const QString &message) {
  logMessage(LogsManager::Warning, QString("请求失败：%1").arg(message));
  emit failed(message);
}

void FanzaScraper::search(const QString &query) {
  const QString keyword = query.trimmed();
  if (keyword.isEmpty()) {
    fail(QStringLiteral("搜索关键词不能为空"));
    return;
  }

  logMessage(LogsManager::Info, QString("开始搜索：%1").arg(keyword));

  QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(keyword));
  encoded.replace(QStringLiteral("%20"), QStringLiteral("+"));
  const QUrl url(
      QString("%1/search/?service=pcgame&searchstr=%2").arg(kBaseUrl, encoded));

  QNetworkReply *reply = _network->get(makeRequest(url));
  connect(reply, &QNetworkReply::finished, this, [this, reply, keyword]() {
    reply->deleteLater();
    const QByteArray payload = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      fail(failureMessage(reply));
      return;
    }

    const QString html = QString::fromUtf8(payload);
    const QList<ScrapeResult> results = parseSearch(html, sourceName());
    if (results.isEmpty()) {
      fail(QStringLiteral("FANZA 未找到匹配项"));
      return;
    }
    logMessage(LogsManager::Info, QString("搜索成功：%1（结果：%2 条）")
                                      .arg(keyword)
                                      .arg(results.size()));
    emit searchFinished(results);
  });
}

void FanzaScraper::fetchDetails(const QString &id) {
  const QString productId = id.trimmed();
  if (productId.isEmpty()) {
    fail(QStringLiteral("结果 ID 为空"));
    return;
  }

  logMessage(LogsManager::Info, QString("开始获取详情：%1").arg(productId));

  const QUrl url(QString("%1/detail/%2/").arg(kBaseUrl, productId));

  QNetworkReply *reply = _network->get(makeRequest(url));
  connect(reply, &QNetworkReply::finished, this, [this, reply, productId]() {
    reply->deleteLater();
    const QByteArray payload = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      fail(failureMessage(reply));
      return;
    }

    const QString html = QString::fromUtf8(payload);
    if (html.contains(QStringLiteral("指定されたページが見つかりません")) ||
        html.contains(QStringLiteral("404 Not Found"))) {
      fail(QStringLiteral("FANZA 未找到该作品"));
      return;
    }

    ScrapeResult result = parseDetails(html, productId, sourceName());
    if (result.title.isEmpty()) {
      fail(QStringLiteral("FANZA 详情解析失败"));
      return;
    }
    logMessage(LogsManager::Info, QString("详情获取成功：%1（背景图 %2 张）")
                                      .arg(productId)
                                      .arg(result.bannerUrls.size()));
    emit detailsReady(result);
  });
}

} // namespace GManager
