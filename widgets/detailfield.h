#ifndef DETAILFIELD_H
#define DETAILFIELD_H

#include <QFont>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include "ElaWidgetToolsDef.h"

class ElaText;

namespace GManager {

// 小标题文字：12px 次要文本色，随主题切换自动更新
class CaptionLabel : public QLabel {
  Q_OBJECT

public:
  explicit CaptionLabel(const QString &text, QWidget *parent = nullptr);

private:
  void applyTheme();

  ElaThemeType::ThemeMode _themeMode;
};

// 1px 分隔线，颜色取主题边框色
class Separator : public QWidget {
  Q_OBJECT

public:
  explicit Separator(QWidget *parent = nullptr);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  ElaThemeType::ThemeMode _themeMode;
};

// 分区标题：左侧强调竖条 + 标题文字
class SectionHeader : public QWidget {
  Q_OBJECT

public:
  explicit SectionHeader(const QString &title, QWidget *parent = nullptr);

  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QString _title;
  ElaThemeType::ThemeMode _themeMode;
};

// 信息字段：上方小标题 + 下方值（值仍为可选中的 ElaText）
class InfoField : public QWidget {
  Q_OBJECT

public:
  explicit InfoField(const QString &label, QWidget *parent = nullptr);

  void setValue(const QString &value);

private:
  ElaText *_value{nullptr};
};

// 评分：五角星 + 数值文本
class RatingBar : public QWidget {
  Q_OBJECT

public:
  explicit RatingBar(QWidget *parent = nullptr);

  void setRating(const QString &rating);

  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  qreal _rating{-1.0};
  QString _text;
  ElaThemeType::ThemeMode _themeMode;
};

// 标签流：圆角小标签，按可用宽度自动换行
class TagFlow : public QWidget {
  Q_OBJECT

public:
  explicit TagFlow(QWidget *parent = nullptr);

  void setTags(const QStringList &tags);

  QSize sizeHint() const override;

protected:
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  struct Chip {
    QString text;
    QRect rect;
  };

  QFont chipFont() const;
  void relayout();

  QStringList _tags;
  QVector<Chip> _chips;
  int _contentHeight{24};
  ElaThemeType::ThemeMode _themeMode;
};

// 游玩状态胶囊：状态色底 + 图标 + 文本
class PlayStatusBadge : public QWidget {
  Q_OBJECT

public:
  explicit PlayStatusBadge(QWidget *parent = nullptr);

  void setStatus(const QString &status);

  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QString _status;
};

} // namespace GManager

#endif // !DETAILFIELD_H
