#include "settingspage.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QVBoxLayout>

#include "ElaLineEdit.h"
#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "ElaScrollPageArea.h"
#include "ElaText.h"
#include "ElaToolButton.h"

#include "appsettings.h"
#include "detailfield.h"
#include "logs.h"

namespace GManager {

namespace {

const QString kUserAgent = QStringLiteral("GManager/0.1");
const QString kTokenPageUrl =
    QStringLiteral("https://next.bgm.tv/demo/access-token");
const QString kMeUrl = QStringLiteral("https://api.bgm.tv/v0/me");
const int kTransferTimeout = 10000;
const int kMessageDuration = 2000;

void logMessage(LogsManager::Level level, const QString &message) {
  LogsManager::getInstance()->logMessage(level, "SettingsPage", message);
}

} // namespace

SettingsPage::SettingsPage(QWidget *parent) : ElaScrollPage(parent) {
  setContentsMargins(0, 20, 0, 0);
  setTitleVisible(false);

  ElaText *titleText = new ElaText("设置", this);
  titleText->setTextPixelSize(30);
  titleText->setTextStyle(ElaTextType::Title);

  QHBoxLayout *titleLayout = new QHBoxLayout();
  titleLayout->setContentsMargins(33, 0, 33, 0);
  titleLayout->addWidget(titleText);
  titleLayout->addStretch();

  QWidget *cardWrapper = new QWidget(this);
  QHBoxLayout *cardLayout = new QHBoxLayout(cardWrapper);
  cardLayout->setContentsMargins(33, 0, 33, 0);
  cardLayout->addWidget(createBangumiCard(), 1);

  QWidget *centralWidget = new QWidget(this);
  QVBoxLayout *centerVLayout = new QVBoxLayout(centralWidget);
  centerVLayout->setSpacing(0);
  centerVLayout->setContentsMargins(0, 0, 0, 0);
  centerVLayout->addLayout(titleLayout);
  centerVLayout->addSpacing(20);
  centerVLayout->addWidget(cardWrapper);
  centerVLayout->addStretch();
  addCentralWidget(centralWidget);

  _network = new QNetworkAccessManager(this);

  loadToken();
}

SettingsPage::~SettingsPage() {}

QWidget *SettingsPage::createBangumiCard() {
  ElaScrollPageArea *card = new ElaScrollPageArea(this);
  QVBoxLayout *cardVLayout = new QVBoxLayout(card);
  cardVLayout->setContentsMargins(20, 16, 20, 18);
  cardVLayout->setSpacing(10);
  // ElaScrollPageArea 构造函数里写死了 setFixedHeight(75)，必须解除，
  // 否则卡片高度被锁在 75px，后面的按钮行与状态行会被裁掉
  card->setMinimumHeight(240);
  card->setMaximumHeight(QWIDGETSIZE_MAX);

  ElaText *hintText = new ElaText(card);
  hintText->setTextPixelSize(13);
  hintText->setWordWrap(true);
  hintText->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  hintText->setText("Bangumi 的 R18 条目在未认证时无法读取，需要填入个人 "
                    "Access Token（账号需注册满 60 天）。Token 只保存在本机 "
                    "config.ini，不写入数据库。");

  _tokenEdit = new ElaLineEdit(card);
  _tokenEdit->setEchoMode(QLineEdit::Password);
  _tokenEdit->setIsClearButtonEnable(true);
  _tokenEdit->setPlaceholderText("粘贴个人 Access Token");
  _tokenEdit->setFixedHeight(35);

  _visibleButton = new ElaToolButton(card);
  _visibleButton->setFixedSize(35, 35);
  _visibleButton->setIsTransparent(false);
  _visibleButton->setElaIcon(ElaIconType::Eye);

  // 用 QWidget 包裹每行，嵌套 QHBoxLayout 直接塞进 QVBoxLayout 时
  // SizeHint 高度会被算成 0，卡片因此被压扁、按钮行被裁掉
  QWidget *tokenRow = new QWidget(card);
  QHBoxLayout *tokenLayout = new QHBoxLayout(tokenRow);
  tokenLayout->setContentsMargins(0, 0, 0, 0);
  tokenLayout->setSpacing(8);
  tokenLayout->addWidget(_tokenEdit, 1);
  tokenLayout->addWidget(_visibleButton);

  ElaPushButton *openPageButton = new ElaPushButton("获取 Token", card);
  _saveButton = new ElaPushButton("保存", card);
  ElaPushButton *clearButton = new ElaPushButton("清空", card);
  openPageButton->setFixedHeight(35);
  _saveButton->setFixedHeight(35);
  clearButton->setFixedHeight(35);

  QWidget *buttonRow = new QWidget(card);
  QHBoxLayout *buttonLayout = new QHBoxLayout(buttonRow);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->setSpacing(8);
  buttonLayout->addWidget(openPageButton);
  buttonLayout->addStretch();
  buttonLayout->addWidget(clearButton);
  buttonLayout->addWidget(_saveButton);

  _statusText = new ElaText(card);
  _statusText->setTextPixelSize(12);
  _statusText->setWordWrap(true);
  _statusText->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

  cardVLayout->addWidget(new SectionHeader("Bangumi", card));
  cardVLayout->addWidget(new Separator(card));
  cardVLayout->addWidget(hintText);
  cardVLayout->addWidget(tokenRow);
  cardVLayout->addWidget(buttonRow);
  cardVLayout->addWidget(_statusText);

  connect(openPageButton, &ElaPushButton::clicked, this,
          [this]() { QDesktopServices::openUrl(QUrl(kTokenPageUrl)); });
  connect(_saveButton, &ElaPushButton::clicked, this, &SettingsPage::saveToken);
  connect(clearButton, &ElaPushButton::clicked, this,
          &SettingsPage::clearToken);
  connect(_visibleButton, &ElaToolButton::clicked, this,
          &SettingsPage::toggleTokenVisible);
  connect(_tokenEdit, &QLineEdit::returnPressed, this,
          &SettingsPage::saveToken);

  return card;
}

void SettingsPage::loadToken() {
  _tokenEdit->setText(AppSettings::getInstance()->bangumiToken());
  updateStatus();
}

void SettingsPage::updateStatus() {
  if (!_statusText) {
    return;
  }

  AppSettings *settings = AppSettings::getInstance();
  if (!settings->hasBangumiToken()) {
    _statusText->setText("未配置 Token：搜索与刮削只覆盖全年龄条目");
    return;
  }

  // 只回显首尾各 4 位，避免整串凭据出现在界面上
  const QString token = settings->bangumiToken();
  if (token.size() <= 8) {
    _statusText->setText("已配置 Token");
    return;
  }
  _statusText->setText(
      QString("已配置 Token：%1…%2").arg(token.left(4), token.right(4)));
}

void SettingsPage::saveToken() {
  const QString token = _tokenEdit->text().trimmed();
  if (token.isEmpty()) {
    clearToken();
    return;
  }

  _pendingToken = token;
  _saveButton->setEnabled(false);
  _statusText->setText("正在验证 Token…");

  QNetworkRequest request{QUrl(kMeUrl)};
  request.setRawHeader("Accept", "application/json");
  request.setRawHeader("User-Agent", kUserAgent.toUtf8());
  request.setRawHeader("Authorization",
                       QString("Bearer %1").arg(token).toUtf8());
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  request.setTransferTimeout(kTransferTimeout);
#endif

  QNetworkReply *reply = _network->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    _saveButton->setEnabled(true);

    const QByteArray payload = reply->readAll();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (status == 401) {
      _statusText->setText("Token 无效或已过期，未保存");
      logMessage(LogsManager::Warning, "Bangumi Token 验证失败：401");
      ElaMessageBar::warning(ElaMessageBarType::TopRight, "验证失败",
                             "Token 无效或已过期", kMessageDuration, this);
      return;
    }

    AppSettings *settings = AppSettings::getInstance();
    settings->setBangumiToken(_pendingToken);

    if (reply->error() != QNetworkReply::NoError && status == 0) {
      // 网络不可用时仍允许保存，避免离线环境无法配置
      updateStatus();
      _statusText->setText(
          QString("%1；暂时无法验证 Token，联网后可重新保存验证")
              .arg(_statusText->text()));
      logMessage(LogsManager::Info, "Bangumi Token 已保存（未验证）");
      ElaMessageBar::warning(ElaMessageBarType::TopRight, "已保存",
                             "暂时无法验证 Token，请联网后重试",
                             kMessageDuration, this);
      return;
    }

    QString nickname;
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (document.isObject()) {
      const QJsonObject user = document.object();
      nickname = user.value(QStringLiteral("nickname")).toString().trimmed();
      if (nickname.isEmpty()) {
        nickname = user.value(QStringLiteral("username")).toString().trimmed();
      }
    }

    updateStatus();
    if (!nickname.isEmpty()) {
      _statusText->setText(
          QString("%1（已验证：%2）").arg(_statusText->text(), nickname));
    }
    logMessage(LogsManager::Info, "Bangumi Token 已更新");
    ElaMessageBar::success(ElaMessageBarType::TopRight, "已保存",
                           "Bangumi Token 已生效", kMessageDuration, this);
  });
}

void SettingsPage::clearToken() {
  AppSettings::getInstance()->setBangumiToken(QString());
  if (_tokenEdit) {
    _tokenEdit->clear();
  }
  updateStatus();
  logMessage(LogsManager::Info, "Bangumi Token 已清空");
  ElaMessageBar::success(ElaMessageBarType::TopRight, "已清空",
                         "Bangumi Token 已移除", kMessageDuration, this);
}

void SettingsPage::toggleTokenVisible() {
  _tokenVisible = !_tokenVisible;
  _tokenEdit->setEchoMode(_tokenVisible ? QLineEdit::Normal
                                        : QLineEdit::Password);
  _visibleButton->setElaIcon(_tokenVisible ? ElaIconType::EyeSlash
                                           : ElaIconType::Eye);
}

} // namespace GManager
