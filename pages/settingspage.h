#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include "ElaScrollPage.h"

class ElaLineEdit;
class ElaPushButton;
class ElaText;
class ElaToolButton;
class QNetworkAccessManager;

namespace GManager {

// 设置页：配置各刮削源凭据（当前只有 Bangumi Access Token）
class SettingsPage : public ElaScrollPage {
  Q_OBJECT

public:
  explicit SettingsPage(QWidget *parent = nullptr);
  ~SettingsPage() override;

private:
  QWidget *createBangumiCard();
  void loadToken();
  void updateStatus();
  void saveToken();
  void clearToken();
  void verifyToken(const QString &token);
  void toggleTokenVisible();

  ElaLineEdit *_tokenEdit{nullptr};
  ElaToolButton *_visibleButton{nullptr};
  ElaText *_statusText{nullptr};
  ElaPushButton *_saveButton{nullptr};
  QNetworkAccessManager *_network{nullptr};
  QString _pendingToken;
  bool _tokenVisible{false};
};

} // namespace GManager

#endif // !SETTINGSPAGE_H
