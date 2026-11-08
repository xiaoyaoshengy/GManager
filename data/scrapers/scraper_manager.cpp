#include "scraper_manager.h"

#include "ElaMessageBar.h"

#include "bangumi_scraper.h"
#include "dlsite_scraper.h"
#include "fanza_scraper.h"
#include "logs.h"
#include "vndb_scraper.h"

namespace GManager {

namespace {

void logMessage(LogsManager::Level level, const QString &message) {
  LogsManager::getInstance()->logMessage(level, "ScraperManager", message);
}

void showWarning(const QString &message) {
  ElaMessageBar::warning(ElaMessageBarType::TopRight, "刮削失败", message,
                         3000);
}

} // namespace

SINGLETON_SOURCE(ScraperManager)

ScraperManager::ScraperManager(QObject *parent) : QObject(parent) {
  logMessage(LogsManager::Info, "刮削器管理器开始加载");
  registerScraper(new VndbScraper(this));
  registerScraper(new BangumiScraper(this));
  registerScraper(new DlsiteScraper(this));
  registerScraper(new FanzaScraper(this));
  logMessage(LogsManager::Info,
             QString("刮削器管理器加载完成，共 %1 个源").arg(_scrapers.size()));
}

ScraperManager::~ScraperManager() {}

void ScraperManager::registerScraper(Scraper *scraper) {
  if (!scraper) {
    logMessage(LogsManager::Warning, "刮削器注册失败：对象为空");
    return;
  }

  const QString name = scraper->sourceName();
  if (name.isEmpty()) {
    logMessage(LogsManager::Warning, "刮削器注册失败：名称为空");
    delete scraper;
    return;
  }
  if (_scrapers.contains(name)) {
    logMessage(LogsManager::Warning,
               QString("刮削器注册失败：源名称重复 %1").arg(name));
    delete scraper;
    return;
  }

  scraper->setParent(this);
  connect(scraper, &Scraper::searchFinished, this,
          [this](const QList<ScrapeResult> &results) {
            logMessage(LogsManager::Info,
                       QString("搜索成功，共 %1 条结果").arg(results.size()));
            emit searchFinished(results);
          });
  connect(scraper, &Scraper::detailsReady, this,
          [this, name](const ScrapeResult &result) {
            logMessage(LogsManager::Info,
                       QString("%1 详情获取成功：%2").arg(name, result.id));
            emit detailsReady(name, result);
          });
  connect(scraper, &Scraper::failed, this, [this, name](const QString &error) {
    logMessage(LogsManager::Warning,
               QString("%1 请求失败：%2").arg(name, error));
    showWarning(error);
    emit scrapeFailed(name, error);
  });
  _scrapers.insert(name, scraper);
  logMessage(LogsManager::Info, QString("已注册刮削器：%1").arg(name));
}

QStringList ScraperManager::availableScrapers() const {
  QStringList names = _scrapers.keys();
  names.sort();
  return names;
}

Scraper *ScraperManager::scraper(const QString &sourceName) const {
  return _scrapers.value(sourceName, nullptr);
}

bool ScraperManager::search(const QString &sourceName, const QString &query) {
  Scraper *target = scraper(sourceName);
  if (!target) {
    logMessage(LogsManager::Warning,
               QString("未找到刮削器：%1").arg(sourceName));
    return false;
  }

  logMessage(LogsManager::Info,
             QString("开始搜索：%1，关键词：%2").arg(sourceName, query));
  target->search(query);
  return true;
}

bool ScraperManager::fetchDetails(const QString &sourceName,
                                  const QString &id) {
  Scraper *target = scraper(sourceName);
  if (!target) {
    logMessage(LogsManager::Warning,
               QString("未找到刮削器：%1").arg(sourceName));
    return false;
  }

  logMessage(LogsManager::Info,
             QString("开始获取详情：%1，ID：%2").arg(sourceName, id));
  target->fetchDetails(id);
  return true;
}

} // namespace GManager
