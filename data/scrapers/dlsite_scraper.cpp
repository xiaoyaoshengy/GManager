#include "dlsite_scraper.h"

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

const QString kBaseUrl = QStringLiteral("https://www.dlsite.com");
const QString kUserAgent = QStringLiteral(
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/96.0.4664.110 Safari/537.36");

void logMessage(LogsManager::Level level, const QString &message) {
  LogsManager::getInstance()->logMessage(level, "DlsiteScraper", message);
}

QString failureMessage(QNetworkReply *reply) {
  if (reply->error() == QNetworkReply::TimeoutError ||
      reply->error() == QNetworkReply::OperationCanceledError) {
    return QStringLiteral("DLsite 请求超时，请检查网络后重试");
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
  request.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,ja;q=0.8,en;q=0.7");
  request.setRawHeader("Cookie", "adultchecked=1");
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  request.setTransferTimeout(15000);
#endif
  return request;
}

QString classInnerHtml(const QString &html, const QString &className) {
  const QRegularExpression regex(
      QStringLiteral("<[^>]*class=\"[^\"]*%1[^\"]*\"[^>]*>(.*?)"
                     "</(?:div|dd|dt|span|p|ul)\\s*>")
          .arg(className),
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::DotMatchesEverythingOption);
  return scraper_utils::firstCapture(
      html, regex.pattern(), 1,
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::DotMatchesEverythingOption);
}

QStringList linkTexts(const QString &html) {
  QStringList out;
  for (const QString &inner :
       scraper_utils::captures(html, QStringLiteral("<a[^>]*>(.*?)</a\\s*>"))) {
    const QString text = scraper_utils::plainText(inner);
    if (!text.isEmpty() && !out.contains(text)) {
      out.append(text);
    }
  }
  return out;
}

// 新版详情页的信息列：dt.c-productInfo__key + dd.c-productInfo__value，
// 按 dt 文本找对应行的 dd 内部 HTML
QString infoRowHtml(const QString &html, const QStringList &keys) {
  static const QRegularExpression rowRe(
      QStringLiteral("<dt[^>]*>([^<]*)</dt\\s*>\\s*<dd[^>]*>(.*?)</dd\\s*>"),
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::DotMatchesEverythingOption);
  QRegularExpressionMatchIterator it = rowRe.globalMatch(html);
  while (it.hasNext()) {
    const QRegularExpressionMatch row = it.next();
    const QString key = scraper_utils::plainText(row.captured(1));
    for (const QString &wanted : keys) {
      if (key.contains(wanted, Qt::CaseInsensitive)) {
        return row.captured(2);
      }
    }
  }
  return QString();
}

// 作品介绍取 div.c-productInfo（“作品内容”区块）的整块内容，
// 清洗交给 optimizeDescription()
QString descriptionHtml(const QString &html) {
  const int markerPos = html.indexOf(QStringLiteral("class=\"c-productInfo\""));
  if (markerPos < 0) {
    return QString();
  }
  return scraper_utils::containerInnerHtml(html, markerPos);
}

// 背景图候选：slider 里带 _img_main / _img_smp 的作品图，
// 跳过缩略图（/resize/）与剧情插入图（/parts/）；优先 jpg，避免依赖 webp 插件
QStringList bannerCandidates(const QString &html) {
  QStringList out;
  const auto collect = [&out](const QStringList &urls) {
    for (const QString &raw : urls) {
      const QString url = scraper_utils::absoluteUrl(raw.trimmed());
      if (!(url.contains(QStringLiteral("_img_main")) ||
            url.contains(QStringLiteral("_img_smp")))) {
        continue;
      }
      if (url.contains(QStringLiteral("/parts/")) ||
          url.contains(QStringLiteral("/resize/")) ||
          url.endsWith(QStringLiteral(".webp"))) {
        continue;
      }
      if (!out.contains(url)) {
        out.append(url);
      }
    }
  };
  collect(scraper_utils::captures(
      html, QStringLiteral("<img[^>]*\\ssrc=\"([^\"]+)\"")));
  if (out.isEmpty()) {
    collect(scraper_utils::captures(
        html, QStringLiteral("<source[^>]*\\ssrcset=\"([^\"]+)\"")));
  }
  return out;
}

// 封面：og:image 就是作品主图，比在 slider 里挑更稳
QString ogImageUrl(const QString &html) {
  QString url = scraper_utils::firstCapture(
      html, QStringLiteral(
                "<meta[^>]*property=\"og:image\"[^>]*content=\"([^\"]+)\""));
  if (url.isEmpty()) {
    url = scraper_utils::firstCapture(
        html, QStringLiteral(
                  "<meta[^>]*content=\"([^\"]+)\"[^>]*property=\"og:image\""));
  }
  return scraper_utils::absoluteUrl(url);
}

// 新版详情页给出 5 分制平均分
QString averageRatingText(const QString &html) {
  return scraper_utils::plainText(scraper_utils::firstCapture(
      html,
      QStringLiteral("<p[^>]*class=\"[^\"]*c-ratingDetailTable__average[^\"]*\""
                     "[^>]*>([^<]*)</p\\s*>")));
}

QList<ScrapeResult> parseSearch(const QString &html, const QString &source) {
  QList<ScrapeResult> results;
  static const QRegularExpression blockRe(
      QStringLiteral("<[a-zA-Z0-9]+[^>]*class=\"[^\"]*"
                     "search_result_img_box_inner[^\"]*\"[^>]*>"),
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

    QString id = scraper_utils::attribute(
        matches.at(i).captured(0), QStringLiteral("data-list_item_product_id"));
    if (id.isEmpty()) {
      id = scraper_utils::firstCapture(
          block, QStringLiteral("product_id/([^.\"'/]+)\\.html"));
    }
    if (id.isEmpty()) {
      continue;
    }

    QString name = scraper_utils::firstCapture(
        block, QStringLiteral("<a[^>]*title=\"([^\"]+)\""));
    if (name.isEmpty()) {
      name = scraper_utils::plainText(
          classInnerHtml(block, QStringLiteral("work_name")));
    }
    if (name.isEmpty()) {
      continue;
    }

    ScrapeResult result;
    result.id = id;
    result.source = source;
    result.title =
        scraper_utils::collapseWhitespace(scraper_utils::decodeEntities(name));
    const QString makerHtml =
        classInnerHtml(block, QStringLiteral("maker_name"));
    result.developer = scraper_utils::plainText(makerHtml);
    results.append(result);
  }
  return results;
}

ScrapeResult parseDetails(const QString &html, const QString &id,
                          const QString &source, const RatingRange &range) {
  ScrapeResult result;
  result.id = id;
  result.source = source;

  result.title = scraper_utils::plainText(scraper_utils::firstCapture(
      html,
      QStringLiteral(
          "<[^>]*id=\"work_name\"[^>]*>(.*?)</(?:h1|h2|span|div)\\s*>"),
      1,
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::DotMatchesEverythingOption));

  const QString rawDescription =
      scraper_utils::sanitizeDescriptionHtml(descriptionHtml(html));
  result.description = scraper_utils::optimizeDescription(rawDescription);

  // 制作商：信息列 サークル名 行里的 a.c-productInfo__makerName
  const QString makerHtml = infoRowHtml(
      html, {QStringLiteral("サークル名"), QStringLiteral("ブランド名")});
  result.developer = scraper_utils::plainText(makerHtml);
  const QString makerHref = scraper_utils::absoluteUrl(
      scraper_utils::attribute(makerHtml, QStringLiteral("href")));

  result.tags = linkTexts(infoRowHtml(html, {QStringLiteral("ジャンル")}))
                    .join(QStringLiteral("、"));
  result.categories = linkTexts(infoRowHtml(html, {QStringLiteral("作品形式"),
                                                   QStringLiteral("类型"),
                                                   QStringLiteral("Genre")}))
                          .join(QStringLiteral("、"));

  QStringList links;
  links.append(
      QString("%1/maniax/work/=/product_id/%2.html").arg(kBaseUrl, id));
  if (!makerHref.isEmpty()) {
    links.append(makerHref);
  }
  links.removeDuplicates();
  result.links = links.join(QLatin1Char('\n'));

  // 背景图：slider 里的作品主图与样图
  result.bannerUrls = bannerCandidates(html);

  // 封面：og:image（作品主图），否则用首张背景图
  result.coverUrl = ogImageUrl(html);
  if (result.coverUrl.isEmpty() && !result.bannerUrls.isEmpty()) {
    result.coverUrl = result.bannerUrls.first();
  }

  // 评分：新版详情页提供 5 分制平均分，无人评分时没有该元素
  bool ratingOk = false;
  const double ratingValue = averageRatingText(html).toDouble(&ratingOk);
  if (ratingOk) {
    result.rating = normalizeRating(QJsonValue(ratingValue), range);
  }

  return result;
}

} // namespace

DlsiteScraper::DlsiteScraper(QObject *parent) : Scraper(parent) {
  logMessage(LogsManager::Info, "DlsiteScraper 开始加载");
  _network = new QNetworkAccessManager(this);
  // 关闭自动 Cookie 罐：否则首个响应的 Set-Cookie 会覆盖手写的 Cookie 头，
  // 导致后续请求丢失成人校验 Cookie（成年确认 / 语言等）
  _network->setCookieJar(nullptr);
}

DlsiteScraper::~DlsiteScraper() {}

QString DlsiteScraper::sourceName() const { return QStringLiteral("dlsite"); }

RatingRange DlsiteScraper::ratingRange() const {
  // 详情页的平均分本身就是 5 分制，换算后数值不变
  return RatingRange{0.0, 5.0};
}

void DlsiteScraper::fail(const QString &message) {
  logMessage(LogsManager::Warning, QString("请求失败：%1").arg(message));
  emit failed(message);
}

void DlsiteScraper::search(const QString &query) {
  const QString keyword = query.trimmed();
  if (keyword.isEmpty()) {
    fail(QStringLiteral("搜索关键词不能为空"));
    return;
  }

  logMessage(LogsManager::Info, QString("开始搜索：%1").arg(keyword));

  QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(keyword));
  encoded.replace(QStringLiteral("%20"), QStringLiteral("+"));
  const QUrl url(QString("%1/maniax/fsr/=/language/jp/keyword/%2/")
                     .arg(kBaseUrl, encoded));

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
      fail(QStringLiteral("DLsite 未找到匹配项"));
      return;
    }
    logMessage(LogsManager::Info, QString("搜索成功：%1（结果：%2 条）")
                                      .arg(keyword)
                                      .arg(results.size()));
    emit searchFinished(results);
  });
}

void DlsiteScraper::fetchDetails(const QString &id) {
  const QString productId = id.trimmed();
  if (productId.isEmpty()) {
    fail(QStringLiteral("结果 ID 为空"));
    return;
  }

  logMessage(LogsManager::Info, QString("开始获取详情：%1").arg(productId));

  const QUrl url(
      QString("%1/maniax/work/=/product_id/%2.html").arg(kBaseUrl, productId));

  QNetworkReply *reply = _network->get(makeRequest(url));
  connect(reply, &QNetworkReply::finished, this, [this, reply, productId]() {
    reply->deleteLater();
    const QByteArray payload = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      fail(failureMessage(reply));
      return;
    }

    const QString html = QString::fromUtf8(payload);
    ScrapeResult result =
        parseDetails(html, productId, sourceName(), ratingRange());
    // 标题解析成功但其它字段全空，说明详情页结构又变了，按失败处理，
    // 避免把只有标题的空壳记录静默入库
    const bool hasDetail =
        !result.description.isEmpty() || !result.developer.isEmpty() ||
        !result.tags.isEmpty() || !result.categories.isEmpty() ||
        !result.bannerUrls.isEmpty();
    if (result.title.isEmpty() || !hasDetail) {
      fail(QStringLiteral("DLsite 详情解析失败"));
      return;
    }
    logMessage(LogsManager::Info,
               QString("详情获取成功：%1（背景图 %2 张，标签 %3 个）")
                   .arg(productId)
                   .arg(result.bannerUrls.size())
                   .arg(result.tags.isEmpty()
                            ? 0
                            : result.tags.count(QStringLiteral("、")) + 1));
    emit detailsReady(result);
  });
}

} // namespace GManager
