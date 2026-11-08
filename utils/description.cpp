#include "description.h"

#include <QRegularExpression>
#include <QStringList>

namespace GManager {

namespace {

// 块级标签：出现时说明内容已有段落结构，换行处理方式与纯文本不同
const QRegularExpression &blockTagRegex() {
  static const QRegularExpression regex(
      QStringLiteral(
          "<\\s*(p|div|ul|ol|li|pre|table|tr|td|h[1-6]|blockquote)\\b"),
      QRegularExpression::CaseInsensitiveOption);
  return regex;
}

// 内容清洗：把富文本整理成适合 QTextDocument 显示的 HTML。
// 顺序：去注释/脚本/iframe → 去图片链接 → 剥离布局 div → 去空块 →
// 处理换行 → 合并连续 <br> → 去块首尾多余 <br>（详见 ai/data.md）。
QString normalizeHtml(const QString &html) {
  QString text = html;
  text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

  // 注释
  static const QRegularExpression comment(
      QStringLiteral("<!--.*?-->"),
      QRegularExpression::DotMatchesEverythingOption);
  text.remove(comment);

  // script / style 区块
  static const QRegularExpression scriptStyle(
      QStringLiteral("<(script|style)\\b[^>]*>.*?</\\1\\s*>"),
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::DotMatchesEverythingOption);
  text.remove(scriptStyle);

  // iframe（QTextDocument 不支持，成对与自闭合都删）
  static const QRegularExpression iframeBlock(
      QStringLiteral("<iframe\\b[^>]*>.*?</iframe\\s*>"),
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::DotMatchesEverythingOption);
  text.remove(iframeBlock);
  static const QRegularExpression iframeTag(
      QStringLiteral("</?iframe\\b[^>]*>"),
      QRegularExpression::CaseInsensitiveOption);
  text.remove(iframeTag);

  // 去掉仅包裹图片的链接，避免点击图片跳转
  static const QRegularExpression linkedImage(
      QStringLiteral("<a\\b[^>]*>\\s*(<img\\b[^>]*>)\\s*</a>"),
      QRegularExpression::CaseInsensitiveOption);
  text.replace(linkedImage, QStringLiteral("\\1"));

  // 剥离纯布局用的 div 外壳，只保留内部内容
  static const QRegularExpression divTag(
      QStringLiteral("</?div\\b[^>]*>"),
      QRegularExpression::CaseInsensitiveOption);
  text.remove(divTag);

  // 删除空块（仅含空白或 <br> 的 p、li）
  static const QRegularExpression emptyBlock(
      QStringLiteral("<(p|li)\\b[^>]*>(?:\\s*<br\\s*/?>)*\\s*</\\1\\s*>"),
      QRegularExpression::CaseInsensitiveOption);
  text.remove(emptyBlock);

  // 换行：有块级标签时源换行只是排版空白，直接丢弃；
  // 纯文本才把换行转成 <br> 以保留手动换行
  static const QRegularExpression blankLines(
      QStringLiteral("\\n[ \\t]*\\n(?:[ \\t]*\\n)+"));
  static const QRegularExpression lineBreak(QStringLiteral("\n\n"));
  static const QRegularExpression singleBreak(QStringLiteral("\\n"));
  if (blockTagRegex().match(text).hasMatch()) {
    // 有块级结构时，源码换行与缩进只是排版空白，一并丢弃
    static const QRegularExpression indentLine(QStringLiteral("\\n[ \\t]*"));
    text.remove(indentLine);
  } else {
    text.replace(blankLines, QStringLiteral("\n\n"));
    text.replace(lineBreak, QStringLiteral("<br><br>"));
    text.replace(singleBreak, QStringLiteral("<br>"));
  }

  // 清除标签之间与多余的空白
  static const QRegularExpression tagGap(
      QStringLiteral(">\\s+<"), QRegularExpression::CaseInsensitiveOption);
  text.replace(tagGap, QStringLiteral("><"));
  static const QRegularExpression extraSpace(QStringLiteral("[ \\t]{2,}"));
  text.replace(extraSpace, QStringLiteral(" "));

  // 合并连续 <br> 为单个
  static const QRegularExpression brRuns(
      QStringLiteral("(?:<br\\s*/?>\\s*){2,}"),
      QRegularExpression::CaseInsensitiveOption);
  text.replace(brRuns, QStringLiteral("<br>"));

  // 去掉块级标签内首尾多余的 <br>
  static const QRegularExpression brBeforeClose(
      QStringLiteral("(?:\\s*<br\\s*/?>)+\\s*(?=</(?:p|li|h[1-6]|ul|ol)\\s*>)"),
      QRegularExpression::CaseInsensitiveOption);
  text.remove(brBeforeClose);
  static const QRegularExpression brAfterOpen(
      QStringLiteral("(<(?:p|li|h[1-6])\\b[^>]*>)(?:\\s*<br\\s*/?>)+"),
      QRegularExpression::CaseInsensitiveOption);
  text.replace(brAfterOpen, QStringLiteral("\\1"));

  // 去掉整体首尾多余的 <br>
  text = text.trimmed();
  static const QRegularExpression leadingBr(
      QStringLiteral("^(?:<br\\s*/?>\\s*)+"),
      QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression trailingBr(
      QStringLiteral("(?:\\s*<br\\s*/?>)+$"),
      QRegularExpression::CaseInsensitiveOption);
  text.remove(leadingBr);
  text.remove(trailingBr);

  return text.trimmed();
}

// 无块级标签时按空行拆段，用 <p> 间距代替连续 <br> 提升可读性
QString paragraphsHtml(const QString &html) {
  if (blockTagRegex().match(html).hasMatch()) {
    return html;
  }

  static const QRegularExpression paragraphBreak(
      QStringLiteral("(?:<br\\s*/?>\\s*){2,}"),
      QRegularExpression::CaseInsensitiveOption);
  QStringList paragraphs;
  for (const QString &part : html.split(paragraphBreak)) {
    const QString trimmed = part.trimmed();
    if (!trimmed.isEmpty()) {
      paragraphs.append(QStringLiteral("<p>%1</p>").arg(trimmed));
    }
  }
  return paragraphs.isEmpty() ? html : paragraphs.join(QString());
}

} // namespace

bool hasBlockLevelTags(const QString &html) {
  return blockTagRegex().match(html).hasMatch();
}

QString optimizeDescription(const QString &html) {
  return paragraphsHtml(normalizeHtml(html));
}

} // namespace GManager
