#ifndef EDITGAMEDIALOG_H
#define EDITGAMEDIALOG_H

#include "ElaDialog.h"

#include "database.h"

class ElaComboBox;
class ElaLineEdit;
class ElaPlainTextEdit;

namespace GManager {

// 修改游戏信息对话框：编辑文字类字段，确认后由调用方比较并写库
class EditGameDialog : public ElaDialog {
  Q_OBJECT

public:
  explicit EditGameDialog(const GameInfo &game, QWidget *parent = nullptr);
  ~EditGameDialog() override;

  // 编辑后的游戏信息，保留 id/cover/banner 原值
  GameInfo updatedGame() const;

private:
  void confirmSelection();
  // 从刮削器检索并预览改动，用户逐字段选择后回填编辑器
  void scrapeFromSource();
  // 只写回与基线不同的字段，避免未选中的文本被 trim/清洗改动
  void applyGameToEditors(const GameInfo &merged, const GameInfo &base);

  GameInfo _source;
  ElaLineEdit *_titleEdit{nullptr};
  ElaLineEdit *_ratingEdit{nullptr};
  ElaLineEdit *_developerEdit{nullptr};
  ElaLineEdit *_tagsEdit{nullptr};
  ElaLineEdit *_categoriesEdit{nullptr};
  ElaComboBox *_statusCombo{nullptr};
  ElaPlainTextEdit *_linksEdit{nullptr};
  ElaPlainTextEdit *_descriptionEdit{nullptr};
};

} // namespace GManager

#endif // !EDITGAMEDIALOG_H
