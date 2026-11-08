#include "scrapepreviewdialog.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include "ElaRadioButton.h"
#include "ElaText.h"

#include "scraper_utils.h"

namespace GManager {

namespace {

const int kCaptionWidth = 64;
const int kOptionTextLimit = 48;
const int kDescriptionPreviewLimit = 600;

// 选项上的值预览：压平换行并截断，完整内容放到 tooltip
QString displayText(const QString &value) {
  const QString text = value.simplified();
  if (text.isEmpty()) {
    return QStringLiteral("（空）");
  }
  if (text.size() > kOptionTextLimit) {
    return text.left(kOptionTextLimit) + QStringLiteral("…");
  }
  return text;
}

QStringList splitValues(const QString &value, const QRegularExpression &sep) {
  QStringList values;
  for (const QString &part : value.split(sep, Qt::SkipEmptyParts)) {
    const QString item = part.trimmed();
    if (!item.isEmpty() && !values.contains(item)) {
      values.append(item);
    }
  }
  return values;
}

// 标签合并：按编辑框约定的分隔符拆分，去重后以顿号连接
QString mergeTags(const QString &original, const QString &scraped) {
  static const QRegularExpression separator(QStringLiteral("[、,，;；/|]"));
  QStringList merged = splitValues(original, separator);
  for (const QString &tag : splitValues(scraped, separator)) {
    if (!merged.contains(tag)) {
      merged.append(tag);
    }
  }
  return merged.join(QStringLiteral("、"));
}

// 相关链接合并：按行拆分，去重后仍按行保存
QString mergeLinks(const QString &original, const QString &scraped) {
  QStringList merged;
  for (const QString &source : {original, scraped}) {
    for (const QString &line :
         source.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
      const QString url = line.trimmed();
      if (!url.isEmpty() && !merged.contains(url)) {
        merged.append(url);
      }
    }
  }
  return merged.join(QLatin1Char('\n'));
}

// 刮削源返回的字段可能是原始 HTML，预览时只展示可读文本
QString readableText(const QString &value) {
  const QString plain = scraper_utils::plainText(value);
  return plain.isEmpty() ? value.simplified() : plain;
}

} // namespace

ScrapePreviewDialog::ScrapePreviewDialog(const GameInfo &current,
                                         const ScrapeResult &scraped,
                                         QWidget *parent)
    : ElaDialog(parent), _current(current), _scraped(scraped) {
  setWindowTitle("刮削预览");
  setFixedSize(820, 780);
  setWindowButtonFlag(ElaAppBarType::StayTopButtonHint, false);
  setWindowButtonFlag(ElaAppBarType::MinimizeButtonHint, false);
  setWindowButtonFlag(ElaAppBarType::MaximizeButtonHint, false);
  setIsFixedSize(true);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(20, getAppBarHeight() + 8, 20, 20);
  mainLayout->setSpacing(10);

  QString sourceName = scraped.source.trimmed();
  if (sourceName.isEmpty()) {
    sourceName = QStringLiteral("未知");
  }
  QString header = QString("刮削源：%1").arg(sourceName.toUpper());
  if (!scraped.id.isEmpty()) {
    header += QString("（ID %1）").arg(scraped.id);
  }

  QHBoxLayout *headerLayout = new QHBoxLayout();
  headerLayout->setContentsMargins(0, 0, 0, 0);
  headerLayout->setSpacing(8);
  ElaText *sourceText = new ElaText(header, this);
  sourceText->setTextPixelSize(13);
  headerLayout->addWidget(sourceText);
  headerLayout->addStretch();

  ElaPushButton *allOriginalButton = new ElaPushButton("全部原始", this);
  ElaPushButton *allScrapedButton = new ElaPushButton("全部刮削", this);
  allOriginalButton->setFixedHeight(32);
  allScrapedButton->setFixedHeight(32);
  allOriginalButton->setToolTip("所有字段都保留原始信息");
  allScrapedButton->setToolTip("所有有刮削值的字段都改用刮削器信息");
  headerLayout->addWidget(allOriginalButton);
  headerLayout->addWidget(allScrapedButton);

  QScrollArea *scrollArea = new QScrollArea(this);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  QWidget *content = new QWidget(scrollArea);
  QVBoxLayout *rowsLayout = new QVBoxLayout(content);
  rowsLayout->setContentsMargins(0, 0, 0, 0);
  rowsLayout->setSpacing(14);
  scrollArea->setWidget(content);

  addFieldRow(rowsLayout, Field::Title, "名称", _current.title, _scraped.title,
              false);
  addFieldRow(rowsLayout, Field::Rating, "评分", _current.rating,
              _scraped.rating, false);
  addFieldRow(rowsLayout, Field::Developer, "开发者", _current.developer,
              _scraped.developer, false);
  addFieldRow(rowsLayout, Field::Tags, "标签", _current.tags, _scraped.tags,
              true);
  addFieldRow(rowsLayout, Field::Categories, "类别", _current.categories,
              _scraped.categories, false);
  addFieldRow(rowsLayout, Field::Links, "相关链接", _current.links,
              _scraped.links, true);
  addFieldRow(rowsLayout, Field::Description, "详细介绍", _current.description,
              _scraped.description, false);
  rowsLayout->addStretch();

  ElaText *previewCaption = new ElaText("介绍预览（选中项）", this);
  previewCaption->setTextPixelSize(12);
  _descriptionPreview = new ElaPlainTextEdit(this);
  _descriptionPreview->setReadOnly(true);
  _descriptionPreview->setFixedHeight(110);
  _descriptionPreview->setTabChangesFocus(true);
  _descriptionPreview->setTextInteractionFlags(Qt::TextSelectableByMouse);

  ElaText *hint = new ElaText(
      "预览阶段不会改动游戏数据，应用后仍需在修改对话框中点击确认", this);
  hint->setTextPixelSize(12);

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  ElaPushButton *cancelButton = new ElaPushButton("取消", this);
  ElaPushButton *applyButton = new ElaPushButton("应用", this);
  cancelButton->setFixedHeight(35);
  applyButton->setFixedHeight(35);
  buttonLayout->addStretch();
  buttonLayout->addWidget(cancelButton);
  buttonLayout->addWidget(applyButton);

  mainLayout->addLayout(headerLayout);
  mainLayout->addWidget(scrollArea, 1);
  mainLayout->addWidget(previewCaption);
  mainLayout->addWidget(_descriptionPreview);
  mainLayout->addWidget(hint);
  mainLayout->addLayout(buttonLayout);

  connect(allOriginalButton, &QPushButton::clicked, this,
          [this]() { applyToAll(kChoiceOriginal); });
  connect(allScrapedButton, &QPushButton::clicked, this,
          [this]() { applyToAll(kChoiceScraped); });
  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  connect(applyButton, &QPushButton::clicked, this, &QDialog::accept);

  updateDescriptionPreview();
}

ScrapePreviewDialog::~ScrapePreviewDialog() {}

void ScrapePreviewDialog::addFieldRow(QVBoxLayout *layout, Field field,
                                      const QString &caption,
                                      const QString &original,
                                      const QString &scraped, bool allowMerge) {
  QWidget *rowWidget = new QWidget(this);
  QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->setSpacing(12);

  ElaText *captionText = new ElaText(caption, rowWidget);
  captionText->setTextPixelSize(13);
  captionText->setFixedWidth(kCaptionWidth);
  rowLayout->addWidget(captionText, 0, Qt::AlignTop);

  QVBoxLayout *optionsLayout = new QVBoxLayout();
  optionsLayout->setContentsMargins(0, 0, 0, 0);
  optionsLayout->setSpacing(4);
  rowLayout->addLayout(optionsLayout, 1);

  FieldRow row;
  row.field = field;
  row.original = original;
  row.scraped = scraped;
  if (allowMerge && !original.trimmed().isEmpty() &&
      !scraped.trimmed().isEmpty()) {
    row.merged = field == Field::Links ? mergeLinks(original, scraped)
                                       : mergeTags(original, scraped);
  }

  QButtonGroup *group = new QButtonGroup(rowWidget);
  row.group = group;

  auto labelFor = [field](const QString &prefix, const QString &value) {
    if (field == Field::Description) {
      const QString text = readableText(value).trimmed();
      return text.isEmpty()
                 ? QString("%1（空）").arg(prefix)
                 : QString("%1（%2 字）").arg(prefix).arg(text.size());
    }
    return QString("%1：%2").arg(prefix, displayText(value));
  };

  auto addOption = [&](int choice, const QString &text, const QString &value,
                       bool enabled) {
    ElaRadioButton *radio = new ElaRadioButton(text, rowWidget);
    radio->setEnabled(enabled);
    radio->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    if (!value.trimmed().isEmpty()) {
      radio->setToolTip(value.left(1000));
    }
    group->addButton(radio, choice);
    optionsLayout->addWidget(radio);
    connect(radio, &QRadioButton::toggled, this, [this](bool checked) {
      if (checked) {
        updateDescriptionPreview();
      }
    });
  };

  const bool hasOriginal = !original.trimmed().isEmpty();
  const bool hasScraped = !scraped.trimmed().isEmpty();

  addOption(kChoiceOriginal, labelFor("原始", original), original, hasOriginal);
  addOption(kChoiceScraped, labelFor("刮削器", scraped), scraped, hasScraped);
  if (!row.merged.isEmpty()) {
    addOption(kChoiceMerged, labelFor("合并", row.merged), row.merged, true);
  }

  int defaultChoice = kChoiceOriginal;
  if (hasScraped && scraped.trimmed() != original.trimmed()) {
    defaultChoice = kChoiceScraped;
  }
  QAbstractButton *defaultButton = group->button(defaultChoice);
  if (!defaultButton || !defaultButton->isEnabled()) {
    defaultButton = group->button(kChoiceOriginal);
  }
  if (defaultButton) {
    defaultButton->setChecked(true);
  }

  layout->addWidget(rowWidget);
  _rows.append(row);
}

void ScrapePreviewDialog::applyToAll(int choice) {
  for (const FieldRow &row : _rows) {
    QAbstractButton *button = row.group->button(choice);
    if (button && button->isEnabled()) {
      button->setChecked(true);
    }
  }
}

QString ScrapePreviewDialog::currentValue(const FieldRow &row) const {
  switch (row.group->checkedId()) {
  case kChoiceScraped:
    return row.scraped;
  case kChoiceMerged:
    return row.merged;
  default:
    return row.original;
  }
}

void ScrapePreviewDialog::updateDescriptionPreview() {
  if (!_descriptionPreview) {
    return;
  }

  for (const FieldRow &row : _rows) {
    if (row.field != Field::Description) {
      continue;
    }
    const QString value = currentValue(row).trimmed();
    if (value.isEmpty()) {
      _descriptionPreview->setPlainText("（无详细介绍）");
      return;
    }
    QString text = readableText(value);
    if (text.size() > kDescriptionPreviewLimit) {
      text = text.left(kDescriptionPreviewLimit) + QStringLiteral("…");
    }
    _descriptionPreview->setPlainText(text);
    return;
  }
}

GameInfo ScrapePreviewDialog::mergedGame() const {
  GameInfo game = _current;
  for (const FieldRow &row : _rows) {
    const QString value = currentValue(row);
    switch (row.field) {
    case Field::Title:
      game.title = value;
      break;
    case Field::Rating:
      game.rating = value;
      break;
    case Field::Developer:
      game.developer = value;
      break;
    case Field::Tags:
      game.tags = value;
      break;
    case Field::Categories:
      game.categories = value;
      break;
    case Field::Links:
      game.links = value;
      break;
    case Field::Description:
      game.description = value;
      break;
    }
  }
  return game;
}

} // namespace GManager
