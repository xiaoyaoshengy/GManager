#include "editgamedialog.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "ElaComboBox.h"
#include "ElaLineEdit.h"
#include "ElaMessageBar.h"
#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include "ElaToolButton.h"

#include "description.h"
#include "playstatus.h"
#include "scrapepreviewdialog.h"
#include "scrapesearchdialog.h"

namespace GManager {

namespace {

const int kMessageDuration = 2000;

void showWarning(const QString &message) {
  ElaMessageBar::warning(ElaMessageBarType::TopRight, "警告", message,
                         kMessageDuration);
}

ElaText *createCaption(const QString &text, QWidget *parent) {
  ElaText *caption = new ElaText(text, parent);
  caption->setTextPixelSize(13);
  return caption;
}

} // namespace

EditGameDialog::EditGameDialog(const GameInfo &game, QWidget *parent)
    : ElaDialog(parent), _source(game) {
  setWindowTitle("修改游戏信息");
  setFixedSize(780, 740);
  setWindowButtonFlag(ElaAppBarType::StayTopButtonHint, false);
  setWindowButtonFlag(ElaAppBarType::MinimizeButtonHint, false);
  setWindowButtonFlag(ElaAppBarType::MaximizeButtonHint, false);
  setIsFixedSize(true);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(20, getAppBarHeight() + 8, 20, 20);
  mainLayout->setSpacing(10);

  QGridLayout *form = new QGridLayout();
  form->setContentsMargins(0, 0, 0, 0);
  form->setHorizontalSpacing(12);
  form->setVerticalSpacing(10);
  form->setColumnMinimumWidth(0, 64);
  form->setColumnStretch(1, 1);

  int row = 0;
  auto addRow = [&](const QString &label, QWidget *editor, bool topAlign) {
    const Qt::Alignment align = topAlign ? (Qt::AlignLeft | Qt::AlignTop)
                                         : (Qt::AlignLeft | Qt::AlignVCenter);
    // 编辑器不加对齐参数，否则布局不会把它拉伸到整列宽度
    editor->setSizePolicy(QSizePolicy::Expanding,
                          editor->sizePolicy().verticalPolicy());
    form->addWidget(createCaption(label, this), row, 0, align);
    form->addWidget(editor, row, 1);
    ++row;
  };

  _titleEdit = new ElaLineEdit(this);
  _titleEdit->setText(_source.title);
  _titleEdit->setPlaceholderText("游戏名称");
  addRow("名称", _titleEdit, false);

  _ratingEdit = new ElaLineEdit(this);
  _ratingEdit->setText(_source.rating);
  _ratingEdit->setPlaceholderText("0~5 之间的数字，留空表示未评分");
  addRow("评分", _ratingEdit, false);

  _developerEdit = new ElaLineEdit(this);
  _developerEdit->setText(_source.developer);
  _developerEdit->setPlaceholderText("开发者或社团名称");
  addRow("开发者", _developerEdit, false);

  _tagsEdit = new ElaLineEdit(this);
  _tagsEdit->setText(_source.tags);
  _tagsEdit->setPlaceholderText("多个标签用顿号或逗号分隔");
  addRow("标签", _tagsEdit, false);

  _categoriesEdit = new ElaLineEdit(this);
  _categoriesEdit->setText(_source.categories);
  _categoriesEdit->setPlaceholderText("类别，例如 ADV");
  addRow("类别", _categoriesEdit, false);

  _statusCombo = new ElaComboBox(this);
  _statusCombo->addItems(playStatusOptions());
  const int statusIndex = playStatusOptions().indexOf(_source.playStatus);
  _statusCombo->setCurrentIndex(statusIndex >= 0 ? statusIndex : 0);
  addRow("游玩状态", _statusCombo, false);

  _linksEdit = new ElaPlainTextEdit(_source.links, this);
  _linksEdit->setPlaceholderText("每行一个链接");
  _linksEdit->setFixedHeight(76);
  _linksEdit->setTabChangesFocus(true);
  addRow("相关链接", _linksEdit, true);

  _descriptionEdit = new ElaPlainTextEdit(_source.description, this);
  _descriptionEdit->setPlaceholderText(
      "支持 HTML，留空时详情页显示“暂无详细介绍”");
  _descriptionEdit->setFixedHeight(200);
  _descriptionEdit->setTabChangesFocus(true);
  addRow("详细介绍", _descriptionEdit, true);

  ElaText *hint = new ElaText("留空的字段在详情页显示为“暂无”", this);
  hint->setTextPixelSize(12);

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  ElaToolButton *scrapeButton = new ElaToolButton(this);
  scrapeButton->setText("刮削");
  scrapeButton->setElaIcon(ElaIconType::CloudArrowDown);
  scrapeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  scrapeButton->setIsTransparent(false);
  ElaPushButton *cancelButton = new ElaPushButton("取消", this);
  ElaPushButton *confirmButton = new ElaPushButton("确认", this);
  scrapeButton->setFixedHeight(35);
  scrapeButton->setToolTip("按名称搜索刮削源，逐字段预览后再决定是否采用");
  cancelButton->setFixedHeight(35);
  confirmButton->setFixedHeight(35);
  buttonLayout->addWidget(scrapeButton);
  buttonLayout->addStretch();
  buttonLayout->addWidget(cancelButton);
  buttonLayout->addWidget(confirmButton);

  mainLayout->addLayout(form);
  mainLayout->addWidget(hint);
  mainLayout->addStretch();
  mainLayout->addLayout(buttonLayout);

  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  connect(confirmButton, &QPushButton::clicked, this,
          &EditGameDialog::confirmSelection);
  connect(scrapeButton, &QPushButton::clicked, this,
          &EditGameDialog::scrapeFromSource);
}

EditGameDialog::~EditGameDialog() {}

GameInfo EditGameDialog::updatedGame() const {
  GameInfo game = _source;
  game.title = _titleEdit->text().trimmed();
  game.rating = _ratingEdit->text().trimmed();
  game.developer = _developerEdit->text().trimmed();
  game.tags = _tagsEdit->text().trimmed();
  game.categories = _categoriesEdit->text().trimmed();
  game.links = _linksEdit->toPlainText().trimmed();
  const QString description = _descriptionEdit->toPlainText().trimmed();
  game.description = hasBlockLevelTags(description)
                         ? optimizeDescription(description)
                         : description;
  game.playStatus = _statusCombo->currentText();
  return game;
}

void EditGameDialog::confirmSelection() {
  const QString rating = _ratingEdit->text().trimmed();
  if (!rating.isEmpty()) {
    bool ok = false;
    const double value = rating.toDouble(&ok);
    if (!ok || value < 0.0 || value > 5.0) {
      showWarning("评分需为 0~5 之间的数字，或留空");
      _ratingEdit->setFocus();
      return;
    }
  }
  accept();
}

void EditGameDialog::scrapeFromSource() {
  // 基线取编辑器当前内容：用户可能已手改但还没确认
  const GameInfo base = updatedGame();

  ScrapeSearchDialog search(_titleEdit->text().trimmed(), this);
  if (search.exec() != QDialog::Accepted) {
    return;
  }

  ScrapePreviewDialog preview(base, search.detail(), this);
  if (preview.exec() != QDialog::Accepted) {
    return;
  }

  applyGameToEditors(preview.mergedGame(), base);
}

void EditGameDialog::applyGameToEditors(const GameInfo &merged,
                                        const GameInfo &base) {
  // 只写回有变化的字段：updatedGame() 会 trim 并清洗介绍，
  // 无条件回写会把用户没选的文本也改掉
  if (merged.title != base.title) {
    _titleEdit->setText(merged.title);
  }
  if (merged.rating != base.rating) {
    _ratingEdit->setText(merged.rating);
  }
  if (merged.developer != base.developer) {
    _developerEdit->setText(merged.developer);
  }
  if (merged.tags != base.tags) {
    _tagsEdit->setText(merged.tags);
  }
  if (merged.categories != base.categories) {
    _categoriesEdit->setText(merged.categories);
  }
  if (merged.links != base.links) {
    _linksEdit->setPlainText(merged.links);
  }
  if (merged.description != base.description) {
    _descriptionEdit->setPlainText(merged.description);
  }
}

} // namespace GManager
