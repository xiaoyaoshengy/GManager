#ifndef SCRAPEPREVIEWDIALOG_H
#define SCRAPEPREVIEWDIALOG_H

#include <QList>
#include <QString>

#include "ElaDialog.h"

#include "database.h"
#include "scraper.h"

class ElaPlainTextEdit;
class QButtonGroup;
class QVBoxLayout;

namespace GManager {

// 刮削结果预览：逐字段在原始值与刮削值之间选择，标签与相关链接可合并。
// 本对话框只做选择，不写库，确认后由调用方决定如何使用结果
class ScrapePreviewDialog : public ElaDialog {
  Q_OBJECT

public:
  ScrapePreviewDialog(const GameInfo &current, const ScrapeResult &scraped,
                      QWidget *parent = nullptr);
  ~ScrapePreviewDialog() override;

  // 以原始信息为底，按各字段的选中项覆盖后的结果
  GameInfo mergedGame() const;

private:
  enum class Field {
    Title,
    Rating,
    Developer,
    Tags,
    Categories,
    Links,
    Description
  };

  enum Choice { kChoiceOriginal = 0, kChoiceScraped = 1, kChoiceMerged = 2 };

  struct FieldRow {
    Field field{Field::Title};
    QButtonGroup *group{nullptr};
    QString original;
    QString scraped;
    QString merged;
  };

  void addFieldRow(QVBoxLayout *layout, Field field, const QString &caption,
                   const QString &original, const QString &scraped,
                   bool allowMerge);
  void applyToAll(int choice);
  void updateDescriptionPreview();
  QString currentValue(const FieldRow &row) const;

  GameInfo _current;
  ScrapeResult _scraped;
  QList<FieldRow> _rows;
  ElaPlainTextEdit *_descriptionPreview{nullptr};
};

} // namespace GManager

#endif // !SCRAPEPREVIEWDIALOG_H
