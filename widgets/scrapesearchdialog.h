#ifndef SCRAPESEARCHDIALOG_H
#define SCRAPESEARCHDIALOG_H

#include <QList>
#include <QString>

#include "ElaDialog.h"

#include "scraper.h"

class ElaComboBox;
class ElaLineEdit;
class ElaListView;
class ElaPushButton;
class ElaText;
class ElaToolButton;
class QStandardItemModel;

namespace GManager {

// 刮削检索对话框：选择刮削源、输入关键词搜索，选中一条结果并取回详情。
// 供修改游戏信息与修改图片两处复用，确认后通过 detail() 取结果
class ScrapeSearchDialog : public ElaDialog {
  Q_OBJECT

public:
  explicit ScrapeSearchDialog(const QString &keyword,
                              QWidget *parent = nullptr);
  ~ScrapeSearchDialog() override;

  // 仅在 exec() 返回 Accepted 后有效
  ScrapeResult detail() const;

private:
  void populateSources();
  void updateMode();
  void startSearch();
  void handleSearchFinished(const QList<ScrapeResult> &results);
  void handleDetailsReady(const QString &sourceName,
                          const ScrapeResult &result);
  void handleFailed(const QString &sourceName, const QString &error);
  void confirmSelection();
  void setBusy(bool busy);
  QString currentSource() const;

  ElaComboBox *_sourceCombo{nullptr};
  ElaLineEdit *_keywordEdit{nullptr};
  ElaToolButton *_searchButton{nullptr};
  ElaListView *_resultView{nullptr};
  QStandardItemModel *_resultModel{nullptr};
  ElaText *_hintText{nullptr};
  ElaPushButton *_confirmButton{nullptr};
  ElaPushButton *_cancelButton{nullptr};
  QList<ScrapeResult> _results;
  ScrapeResult _pending; // 当前选中条目的基础信息
  ScrapeResult _detail;  // 含详情字段的结果
  bool _busy{false};
  bool _awaitingDetails{false};
  bool _fallbackAllowed{false};
};

} // namespace GManager

#endif // !SCRAPESEARCHDIALOG_H
