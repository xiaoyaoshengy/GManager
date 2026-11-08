#ifndef DLSITE_SCRAPER_H
#define DLSITE_SCRAPER_H

#include "scraper.h"

class QNetworkAccessManager;

namespace GManager {

// DLsite 刮削器：抓取 maniax 站点的搜索结果与作品页
class DlsiteScraper : public Scraper {
  Q_OBJECT

public:
  explicit DlsiteScraper(QObject *parent = nullptr);
  ~DlsiteScraper() override;

  QString sourceName() const override;
  RatingRange ratingRange() const override;
  void search(const QString &query) override;
  void fetchDetails(const QString &id) override;

private:
  void fail(const QString &message);

  QNetworkAccessManager *_network{nullptr};
};

} // namespace GManager

#endif // !DLSITE_SCRAPER_H
