#ifndef FANZA_SCRAPER_H
#define FANZA_SCRAPER_H

#include "scraper.h"

class QNetworkAccessManager;

namespace GManager {

// FANZA 刮削器：抓取 dlsoft.dmm.co.jp 的搜索结果与作品页
class FanzaScraper : public Scraper {
  Q_OBJECT

public:
  explicit FanzaScraper(QObject *parent = nullptr);
  ~FanzaScraper() override;

  QString sourceName() const override;
  RatingRange ratingRange() const override;
  void search(const QString &query) override;
  void fetchDetails(const QString &id) override;

private:
  void fail(const QString &message);

  QNetworkAccessManager *_network{nullptr};
};

} // namespace GManager

#endif // !FANZA_SCRAPER_H
