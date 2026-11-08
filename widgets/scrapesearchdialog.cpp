#include "scrapesearchdialog.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QModelIndex>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaListView.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaToolButton.h"

#include "appsettings.h"
#include "logs.h"
#include "scraper_manager.h"

namespace GManager {

namespace {

const int kMessageDuration = 2000;

void showWarning(const QString &message) {
  ElaMessageBar::warning(ElaMessageBarType::TopRight, "警告", message,
                         kMessageDuration);
}

void logMessage(LogsManager::Level level, const QString &message) {
  LogsManager::getInstance()->logMessage(level, "ScrapeSearchDialog", message);
}

} // namespace

ScrapeSearchDialog::ScrapeSearchDialog(const QString &keyword, QWidget *parent)
    : ElaDialog(parent) {
  setWindowTitle("从刮削器获取");
  setFixedSize(620, 520);
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
  _keywordEdit->setText(keyword.trimmed());
  _keywordEdit->setPlaceholderText("输入搜索关键词");
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

  populateSources();
  updateMode();

  connect(_sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int) { updateMode(); });
  connect(_searchButton, &QToolButton::clicked, this,
          &ScrapeSearchDialog::startSearch);
  connect(_keywordEdit, &QLineEdit::returnPressed, this,
          &ScrapeSearchDialog::startSearch);
  connect(_confirmButton, &QPushButton::clicked, this,
          &ScrapeSearchDialog::confirmSelection);
  connect(_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  connect(_resultView, &QListView::doubleClicked, this,
          [this](const QModelIndex &index) {
            _resultView->setCurrentIndex(index);
            confirmSelection();
          });

  ScraperManager *manager = ScraperManager::getInstance();
  connect(manager, &ScraperManager::searchFinished, this,
          &ScrapeSearchDialog::handleSearchFinished);
  connect(manager, &ScraperManager::detailsReady, this,
          &ScrapeSearchDialog::handleDetailsReady);
  connect(manager, &ScraperManager::scrapeFailed, this,
          &ScrapeSearchDialog::handleFailed);
}

ScrapeSearchDialog::~ScrapeSearchDialog() {}

ScrapeResult ScrapeSearchDialog::detail() const { return _detail; }

QString ScrapeSearchDialog::currentSource() const {
  return _sourceCombo->currentData().toString();
}

void ScrapeSearchDialog::populateSources() {
  const QStringList sources =
      ScraperManager::getInstance()->availableScrapers();
  for (const QString &source : sources) {
    _sourceCombo->addItem(source.toUpper(), source);
  }
  if (_sourceCombo->count() > 0) {
    _sourceCombo->setCurrentIndex(0);
  }
}

void ScrapeSearchDialog::updateMode() {
  const QString source = currentSource();
  _busy = false;
  _awaitingDetails = false;
  _fallbackAllowed = false;
  _resultModel->clear();
  _results.clear();
  _pending = ScrapeResult();
  _searchButton->setEnabled(!source.isEmpty());
  _confirmButton->setEnabled(true);

  QString hint = "选择刮削源并输入关键词后点击搜索";
  if (source == QStringLiteral("bangumi") &&
      !AppSettings::getInstance()->hasBangumiToken()) {
    hint += "（未配置 Bangumi Token，仅能刮削全年龄条目，见 设置 → "
            "Bangumi）";
  }
  _hintText->setText(hint);
}

void ScrapeSearchDialog::setBusy(bool busy) {
  _busy = busy;
  _searchButton->setEnabled(!busy && !currentSource().isEmpty());
  _confirmButton->setEnabled(!busy);
}

void ScrapeSearchDialog::startSearch() {
  const QString source = currentSource();
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
  _pending = ScrapeResult();
  _awaitingDetails = false;
  _fallbackAllowed = false;
  setBusy(true);
  _hintText->setText("搜索中…");

  if (!ScraperManager::getInstance()->search(source, keyword)) {
    setBusy(false);
    _hintText->setText("未找到可用的刮削器");
  }
}

void ScrapeSearchDialog::handleSearchFinished(
    const QList<ScrapeResult> &results) {
  if (results.isEmpty()) {
    return;
  }
  // searchFinished 是全局广播且不带源名，只能按结果自带的源过滤
  if (results.first().source != currentSource()) {
    return;
  }

  setBusy(false);
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

void ScrapeSearchDialog::handleDetailsReady(const QString &sourceName,
                                            const ScrapeResult &result) {
  if (sourceName != currentSource() || !_awaitingDetails ||
      _pending.id != result.id) {
    return;
  }

  ScrapeResult detail = result;
  if (detail.title.isEmpty()) {
    detail.title = _pending.title;
  }
  if (detail.coverUrl.isEmpty()) {
    detail.coverUrl = _pending.coverUrl;
  }
  if (detail.bannerUrls.isEmpty()) {
    detail.bannerUrls = _pending.bannerUrls;
  }

  logMessage(LogsManager::Info, QString("详情获取完成：%1（背景图 %2 张）")
                                    .arg(detail.id)
                                    .arg(detail.bannerUrls.size()));
  _detail = detail;
  _awaitingDetails = false;
  setBusy(false);
  accept();
}

void ScrapeSearchDialog::handleFailed(const QString &sourceName,
                                      const QString &error) {
  if (sourceName != currentSource()) {
    return;
  }

  const bool wasDetails = _awaitingDetails;
  _awaitingDetails = false;
  setBusy(false);
  if (wasDetails) {
    _fallbackAllowed = true;
    _hintText->setText(
        QString("%1；再次点击“确认”将只使用搜索结果的基础信息").arg(error));
    return;
  }
  _hintText->setText(error);
}

void ScrapeSearchDialog::confirmSelection() {
  if (_busy) {
    return;
  }

  if (_fallbackAllowed) {
    _detail = _pending;
    _fallbackAllowed = false;
    accept();
    return;
  }

  const QModelIndex index = _resultView->currentIndex();
  if (!index.isValid() || index.row() >= _results.size()) {
    showWarning("请先选择一个搜索结果");
    return;
  }

  const ScrapeResult &result = _results.at(index.row());
  if (result.id.isEmpty()) {
    _detail = result;
    accept();
    return;
  }

  _pending = result;
  _awaitingDetails = true;
  setBusy(true);
  _hintText->setText("正在获取详情…");

  if (!ScraperManager::getInstance()->fetchDetails(currentSource(),
                                                   result.id)) {
    _awaitingDetails = false;
    setBusy(false);
    _detail = result;
    accept();
  }
}

} // namespace GManager
