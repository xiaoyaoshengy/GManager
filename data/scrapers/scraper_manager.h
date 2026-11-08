#ifndef SCRAPER_MANAGER_H
#define SCRAPER_MANAGER_H

#include <QHash>
#include <QStringList>

#include "scraper.h"
#include "singleton_macro.h"

namespace GManager {

// 刮削器注册表：按源名称管理可用刮削器并转发结果
class ScraperManager : public QObject {
  Q_OBJECT

  SINGLETON_HEADER(ScraperManager)

public:
  void registerScraper(Scraper *scraper);
  QStringList availableScrapers() const;
  Scraper *scraper(const QString &sourceName) const;

  bool search(const QString &sourceName, const QString &query);
  bool fetchDetails(const QString &sourceName, const QString &id);

signals:
  void searchFinished(const QList<ScrapeResult> &results);
  void detailsReady(const QString &sourceName, const ScrapeResult &result);
  void scrapeFailed(const QString &sourceName, const QString &error);

private:
  QHash<QString, Scraper *> _scrapers;
};

} // namespace GManager

#endif // !SCRAPER_MANAGER_H
