#ifndef SCRAPER_H
#define SCRAPER_H

#include <QJsonValue>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

namespace GManager {

// 刮削源自己的评分范围，用于换算到程序的 0~5 分制
struct RatingRange {
  double min = 0.0;
  double max = 1.0;
};

// 程序统一评分上限
constexpr double kProgramRatingMax = 5.0;

// 把源评分按 RatingRange 换算到 0~5，保留 1 位小数；
// 非数字或范围非法时返回空字符串
inline QString normalizeRating(const QJsonValue &value,
                               const RatingRange &range) {
  if (!value.isDouble() || range.max <= range.min) {
    return QString();
  }

  const double source = value.toDouble();
  const double clamped = source < range.min
                             ? range.min
                             : (source > range.max ? range.max : source);
  const double program =
      (clamped - range.min) / (range.max - range.min) * kProgramRatingMax;
  return QString::number(program, 'f', 1);
}

// 刮削结果：id 为源内标识，source 为源名称；
// 详情字段在 fetchDetails() 后填充
struct ScrapeResult {
  QString id;
  QString source;
  QString title;
  QString coverUrl;
  QString description;
  QString rating;
  QString developer;
  QString tags;
  QString categories;
  QString links;
  QStringList bannerUrls;
};

// 刮削器接口：各具体源继承本类并实现下述纯虚函数
class Scraper : public QObject {
  Q_OBJECT

public:
  explicit Scraper(QObject *parent = nullptr);
  ~Scraper() override;

  virtual QString sourceName() const = 0;
  virtual RatingRange ratingRange() const = 0;
  virtual void search(const QString &query) = 0;
  virtual void fetchDetails(const QString &id) = 0;

signals:
  void searchFinished(const QList<ScrapeResult> &results);
  void detailsReady(const ScrapeResult &result);
  void failed(const QString &error);
};

} // namespace GManager

#endif // !SCRAPER_H
