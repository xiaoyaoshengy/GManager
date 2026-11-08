#ifndef ADDGAMEDIALOG_H
#define ADDGAMEDIALOG_H

#include <QList>

#include "ElaDialog.h"

#include "scraper.h"

class ElaComboBox;
class ElaLineEdit;
class ElaListView;
class ElaPushButton;
class ElaText;
class ElaToolButton;
class QNetworkAccessManager;
class QStandardItemModel;

namespace GManager {

// 添加游戏对话框：选择刮削器或自定义，搜索并确认入库
class AddGameDialog : public ElaDialog {
  Q_OBJECT

public:
  explicit AddGameDialog(QWidget *parent = nullptr);
  ~AddGameDialog() override;

  QString addedGameId() const;

private:
  void populateSources();
  void updateMode();
  void startSearch();
  void handleSearchFinished(const QList<ScrapeResult> &results);
  void handleDetailsReady(const QString &sourceName,
                          const ScrapeResult &result);
  void handleSearchFailed(const QString &sourceName, const QString &error);
  void confirmSelection();
  void addToLibrary(const ScrapeResult &detail, const QString &bannerUrl);
  void downloadBanner(const ScrapeResult &detail, const QString &bannerUrl);
  void downloadCover(const ScrapeResult &detail, const QString &bannerFile);
  void insertGame(const ScrapeResult &detail, const QString &coverFile,
                  const QString &bannerFile);

  ElaComboBox *_sourceCombo{nullptr};
  ElaLineEdit *_keywordEdit{nullptr};
  ElaToolButton *_searchButton{nullptr};
  ElaListView *_resultView{nullptr};
  QStandardItemModel *_resultModel{nullptr};
  ElaPushButton *_confirmButton{nullptr};
  ElaPushButton *_cancelButton{nullptr};
  ElaText *_hintText{nullptr};
  QNetworkAccessManager *_network{nullptr};
  QList<ScrapeResult> _results;
  ScrapeResult _pendingDetail;
  QString _addedGameId;
  bool _pendingDetailReady{false};
  bool _busy{false};
};

} // namespace GManager

#endif // !ADDGAMEDIALOG_H
