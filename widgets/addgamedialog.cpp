#include "addgamedialog.h"

#include <QAbstractItemView>
#include <QDir>
#include <QHBoxLayout>
#include <QImage>
#include <QModelIndex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardItemModel>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaListView.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaToolButton.h"

#include "appsettings.h"
#include "database.h"
#include "global_defines.h"
#include "imagepickerdialog.h"
#include "logs.h"
#include "playstatus.h"
#include "scraper_manager.h"

namespace GManager {

namespace {

const QString kUserAgent = QStringLiteral("GManager/0.1");
const int kMessageDuration = 2000;

void showWarning(const QString &message) {
  ElaMessageBar::warning(ElaMessageBarType::TopRight, "警告", message,
                         kMessageDuration);
}

void logMessage(LogsManager::Level level, const QString &message) {
  LogsManager::getInstance()->logMessage(level, "AddGameDialog", message);
}

} // namespace

AddGameDialog::AddGameDialog(QWidget *parent) : ElaDialog(parent) {
  setWindowTitle("添加游戏");
  setFixedSize(640, 560);
  setWindowButtonFlag(ElaAppBarType::StayTopButtonHint, false);
  setWindowButtonFlag(ElaAppBarType::MinimizeButtonHint, false);
  setWindowButtonFlag(ElaAppBarType::MaximizeButtonHint, false);
  setIsFixedSize(true);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(20, getAppBarHeight() + 8, 20, 20);
  mainLayout->setSpacing(12);

  QHBoxLayout *searchLayout = new QHBoxLayout();
  _sourceCombo = new ElaComboBox(this);
  _keywordEdit = new ElaLineEdit(this);
  _searchButton = new ElaToolButton(this);
  _searchButton->setFixedSize(35, 35);
  _searchButton->setIsTransparent(false);
  _searchButton->setElaIcon(ElaIconType::MagnifyingGlass);
  searchLayout->addWidget(_sourceCombo, 1);
  searchLayout->addWidget(_keywordEdit, 3);
  searchLayout->addWidget(_searchButton);

  _resultView = new ElaListView(this);
  _resultModel = new QStandardItemModel(this);
  _resultView->setModel(_resultModel);
  _resultView->setItemHeight(44);
  _resultView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  _resultView->setSelectionMode(QAbstractItemView::SingleSelection);

  _hintText = new ElaText(this);
  _hintText->setTextPixelSize(12);

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  _confirmButton = new ElaPushButton("确认", this);
  _cancelButton = new ElaPushButton("取消", this);
  _confirmButton->setFixedHeight(35);
  _cancelButton->setFixedHeight(35);
  buttonLayout->addStretch();
  buttonLayout->addWidget(_cancelButton);
  buttonLayout->addWidget(_confirmButton);

  mainLayout->addLayout(searchLayout);
  mainLayout->addWidget(_resultView, 1);
  mainLayout->addWidget(_hintText);
  mainLayout->addLayout(buttonLayout);

  _network = new QNetworkAccessManager(this);

  populateSources();
  updateMode();

  connect(_sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { updateMode(); });
  connect(_searchButton, &QToolButton::clicked, this,
          &AddGameDialog::startSearch);
  connect(_confirmButton, &QPushButton::clicked, this,
          &AddGameDialog::confirmSelection);
  connect(_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  ScraperManager *manager = ScraperManager::getInstance();
  connect(manager, &ScraperManager::searchFinished, this,
          &AddGameDialog::handleSearchFinished);
  connect(manager, &ScraperManager::detailsReady, this,
          &AddGameDialog::handleDetailsReady);
  connect(manager, &ScraperManager::scrapeFailed, this,
          &AddGameDialog::handleSearchFailed);
}

AddGameDialog::~AddGameDialog() {}

void AddGameDialog::populateSources() {
  const QStringList sources =
      ScraperManager::getInstance()->availableScrapers();
  for (const QString &source : sources) {
    _sourceCombo->addItem(source.toUpper(), source);
  }
  _sourceCombo->addItem("自定义", QString());
  if (_sourceCombo->count() > 0) {
    _sourceCombo->setCurrentIndex(0);
  }
}

void AddGameDialog::updateMode() {
  const QString source = _sourceCombo->currentData().toString();
  const bool custom = source.isEmpty();
  _keywordEdit->setPlaceholderText(custom ? "输入游戏标题" : "输入关键词");
  _busy = false;
  _pendingDetailReady = false;
  _searchButton->setEnabled(!custom);
  _confirmButton->setEnabled(true);
  _resultModel->clear();
  _results.clear();

  QString hint =
      custom ? "自定义模式：输入标题后点击确认" : "输入关键词后点击搜索";
  if (source == QStringLiteral("bangumi") &&
      !AppSettings::getInstance()->hasBangumiToken()) {
    hint += "（未配置 Bangumi Token，仅能刮削全年龄条目，见 设置 → "
            "Bangumi）";
  }
  _hintText->setText(hint);
}

void AddGameDialog::startSearch() {
  const QString source = _sourceCombo->currentData().toString();
  if (source.isEmpty() || _busy) {
    return;
  }

  const QString keyword = _keywordEdit->text().trimmed();
  if (keyword.isEmpty()) {
    showWarning("请输入搜索关键词");
    return;
  }

  _resultModel->clear();
  _results.clear();
  _busy = true;
  _searchButton->setEnabled(false);
  _confirmButton->setEnabled(false);
  _hintText->setText("搜索中…");

  if (!ScraperManager::getInstance()->search(source, keyword)) {
    _busy = false;
    _searchButton->setEnabled(true);
    _confirmButton->setEnabled(true);
    _hintText->setText("未找到可用的刮削器");
  }
}

void AddGameDialog::handleSearchFinished(const QList<ScrapeResult> &results) {
  if (results.isEmpty()) {
    return;
  }
  if (results.first().source != _sourceCombo->currentData().toString()) {
    return;
  }

  _busy = false;
  _searchButton->setEnabled(true);
  _confirmButton->setEnabled(true);

  for (const ScrapeResult &result : results) {
    _results.append(result);
    QStandardItem *item = new QStandardItem(result.title);
    item->setEditable(false);
    _resultModel->appendRow(item);
  }
  if (_resultModel->rowCount() > 0) {
    _resultView->setCurrentIndex(_resultModel->index(0, 0));
  }
  _hintText->setText(
      QString("搜索完成，共 %1 条结果").arg(_resultModel->rowCount()));
}

void AddGameDialog::handleDetailsReady(const QString &sourceName,
                                       const ScrapeResult &result) {
  if (sourceName != _sourceCombo->currentData().toString()) {
    return;
  }
  if (!_pendingDetailReady || _pendingDetail.id != result.id) {
    return;
  }

  ScrapeResult detail = result;
  if (detail.title.isEmpty()) {
    detail.title = _pendingDetail.title;
  }
  if (detail.coverUrl.isEmpty()) {
    detail.coverUrl = _pendingDetail.coverUrl;
  }

  _pendingDetailReady = false;
  _busy = false;
  _confirmButton->setEnabled(true);

  QString bannerUrl;
  if (!detail.bannerUrls.isEmpty()) {
    ImagePickerDialog picker(detail.bannerUrls, this);
    if (picker.exec() == QDialog::Accepted) {
      bannerUrl = picker.selectedUrl();
    }
  }

  addToLibrary(detail, bannerUrl);
}

void AddGameDialog::handleSearchFailed(const QString &sourceName,
                                       const QString &error) {
  if (sourceName != _sourceCombo->currentData().toString()) {
    return;
  }

  _busy = false;
  _searchButton->setEnabled(true);
  _confirmButton->setEnabled(true);

  if (_pendingDetailReady) {
    const ScrapeResult result = _pendingDetail;
    _pendingDetailReady = false;
    _hintText->setText("详情获取失败，使用基础信息");
    addToLibrary(result, QString());
    return;
  }

  _hintText->setText(error);
}

void AddGameDialog::confirmSelection() {
  if (_busy) {
    return;
  }

  const QString source = _sourceCombo->currentData().toString();
  if (source.isEmpty()) {
    const QString title = _keywordEdit->text().trimmed();
    if (title.isEmpty()) {
      showWarning("请输入游戏标题");
      return;
    }
    ScrapeResult detail;
    detail.title = title;
    addToLibrary(detail, QString());
    return;
  }

  const QModelIndex index = _resultView->currentIndex();
  if (!index.isValid() || index.row() >= _results.size()) {
    showWarning("请先选择一个搜索结果");
    return;
  }

  const ScrapeResult &result = _results.at(index.row());
  if (result.id.isEmpty()) {
    addToLibrary(result, QString());
    return;
  }

  _pendingDetail = result;
  _pendingDetailReady = true;
  _busy = true;
  _confirmButton->setEnabled(false);
  _hintText->setText("正在获取详情…");

  if (!ScraperManager::getInstance()->fetchDetails(source, result.id)) {
    _pendingDetailReady = false;
    _busy = false;
    _confirmButton->setEnabled(true);
    addToLibrary(result, QString());
  }
}

void AddGameDialog::addToLibrary(const ScrapeResult &detail,
                                 const QString &bannerUrl) {
  if (bannerUrl.isEmpty()) {
    downloadCover(detail, QString());
    return;
  }

  downloadBanner(detail, bannerUrl);
}

void AddGameDialog::downloadBanner(const ScrapeResult &detail,
                                   const QString &bannerUrl) {
  _busy = true;
  _confirmButton->setEnabled(false);
  _hintText->setText("正在下载背景图…");

  QNetworkRequest request{QUrl(bannerUrl)};
  request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
  QNetworkReply *reply = _network->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, detail]() {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
      logMessage(LogsManager::Warning,
                 QString("背景图下载失败：%1").arg(reply->errorString()));
      showWarning("背景图下载失败，将不设置横幅");
      downloadCover(detail, QString());
      return;
    }

    QImage image;
    if (!image.loadFromData(reply->readAll())) {
      logMessage(LogsManager::Warning, "背景图数据无法解析，将不设置横幅");
      showWarning("背景图数据无法解析，将不设置横幅");
      downloadCover(detail, QString());
      return;
    }

    QDir().mkpath(bannerDir());
    const QString fileName =
        QUuid::createUuid().toString(QUuid::WithoutBraces) + ".jpg";
    if (!image.save(QString("%1/%2").arg(bannerDir(), fileName), "JPG")) {
      logMessage(LogsManager::Warning, "背景图保存失败，将不设置横幅");
      showWarning("背景图保存失败，将不设置横幅");
      downloadCover(detail, QString());
      return;
    }

    downloadCover(detail, fileName);
  });
}

void AddGameDialog::downloadCover(const ScrapeResult &detail,
                                  const QString &bannerFile) {
  if (detail.coverUrl.isEmpty()) {
    insertGame(detail, QString(), bannerFile);
    return;
  }

  _busy = true;
  _confirmButton->setEnabled(false);
  _hintText->setText("正在下载封面…");

  QNetworkRequest request{QUrl(detail.coverUrl)};
  request.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
  QNetworkReply *reply = _network->get(request);
  connect(
      reply, &QNetworkReply::finished, this,
      [this, reply, detail, bannerFile]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
          logMessage(LogsManager::Warning,
                     QString("封面下载失败：%1").arg(reply->errorString()));
          showWarning("封面下载失败，将使用默认封面");
          insertGame(detail, QString(), bannerFile);
          return;
        }

        QImage image;
        if (!image.loadFromData(reply->readAll())) {
          logMessage(LogsManager::Warning, "封面数据无法解析，将使用默认封面");
          showWarning("封面数据无法解析，将使用默认封面");
          insertGame(detail, QString(), bannerFile);
          return;
        }

        QDir().mkpath(coverDir());
        const QString fileName =
            QUuid::createUuid().toString(QUuid::WithoutBraces) + ".jpg";
        if (!image.save(QString("%1/%2").arg(coverDir(), fileName), "JPG")) {
          logMessage(LogsManager::Warning, "封面保存失败，将使用默认封面");
          showWarning("封面保存失败，将使用默认封面");
          insertGame(detail, QString(), bannerFile);
          return;
        }

        insertGame(detail, fileName, bannerFile);
      });
}

void AddGameDialog::insertGame(const ScrapeResult &detail,
                               const QString &coverFile,
                               const QString &bannerFile) {
  _busy = false;
  _confirmButton->setEnabled(true);

  DatabaseManager *database = DatabaseManager::getInstance();
  if (!database->isReady()) {
    showWarning("游戏数据库未就绪");
    return;
  }

  GameInfo game;
  game.title = detail.title;
  game.cover = coverFile;
  game.banner = bannerFile;
  game.description = detail.description;
  game.rating = detail.rating;
  game.developer = detail.developer;
  game.tags = detail.tags;
  game.categories = detail.categories;
  game.links = detail.links;
  game.playStatus = defaultPlayStatus();
  if (!database->insertGame(&game)) {
    _hintText->setText("添加游戏失败");
    return;
  }

  _addedGameId = game.id;
  logMessage(LogsManager::Info,
             QString("添加游戏成功：%1（ID：%2）").arg(detail.title, game.id));
  accept();
}

QString AddGameDialog::addedGameId() const { return _addedGameId; }

} // namespace GManager
