#ifndef VNDB_SCRAPER_H
#define VNDB_SCRAPER_H

#include "scraper.h"

class QNetworkAccessManager;

namespace GManager {

// VNDB 刮削器：通过 Kana API 的 POST /vn 按标题搜索视觉小说
class VndbScraper : public Scraper {
  Q_OBJECT

public:
  explicit VndbScraper(QObject *parent = nullptr);
  ~VndbScraper() override;

  QString sourceName() const override;
  RatingRange ratingRange() const override;
  void search(const QString &query) override;
  void fetchDetails(const QString &id) override;

private:
  void fail(const QString &message);

  QNetworkAccessManager *_network{nullptr};
};

} // namespace GManager

#endif // !VNDB_SCRAPER_H
