#ifndef BANGUMI_SCRAPER_H
#define BANGUMI_SCRAPER_H

#include "scraper.h"

class QNetworkAccessManager;

namespace GManager {

// Bangumi 刮削器：通过 bgm.tv API 搜索并获取条目详情
class BangumiScraper : public Scraper {
  Q_OBJECT

public:
  explicit BangumiScraper(QObject *parent = nullptr);
  ~BangumiScraper() override;

  QString sourceName() const override;
  RatingRange ratingRange() const override;
  void search(const QString &query) override;
  void fetchDetails(const QString &id) override;

private:
  void fail(const QString &message);

  QNetworkAccessManager *_network{nullptr};
};

} // namespace GManager

#endif // !BANGUMI_SCRAPER_H
