#ifndef SCRAPER_UTILS_H
#define SCRAPER_UTILS_H

#include <QChar>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <climits>

namespace GManager {
namespace scraper_utils {

// 解码常见 HTML 实体（含十进制/十六进制数字实体）
inline QString decodeEntities(QString text) {
  text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
  text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
  text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
  text.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
  text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
  text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
  text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));

  static const QRegularExpression numeric(
      QStringLiteral("&#(x[0-9A-Fa-f]+|\\d+);"));
  QString out;
  out.reserve(text.size());
  int last = 0;
  QRegularExpressionMatchIterator it = numeric.globalMatch(text);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    out += text.mid(last, match.capturedStart() - last);
    const QString code = match.captured(1);
    bool ok = false;
    const uint value =
        (code.startsWith(QLatin1Char('x')) || code.startsWith(QLatin1Char('X')))
            ? code.mid(1).toUInt(&ok, 16)
            : code.toUInt(&ok, 10);
    out +=
        ok ? QString(QChar(static_cast<char16_t>(value))) : match.captured(0);
    last = match.capturedEnd();
  }
  out += text.mid(last);
  return out;
}

// 压缩空白（含全角空格）并去掉首尾
inline QString collapseWhitespace(QString text) {
  static const QRegularExpression ws(QStringLiteral("[\\s\\x{3000}]+"));
  text.replace(ws, QStringLiteral(" "));
  return text.trimmed();
}

// 去标签后取纯文本
inline QString plainText(const QString &html) {
  static const QRegularExpression tag(QStringLiteral("<[^>]*>"));
  QString text = html;
  text.remove(tag);
  return collapseWhitespace(decodeEntities(text));
}

// 协议相对地址 //host 补成 https://host
inline QString absoluteUrl(const QString &url) {
  const QString trimmed = url.trimmed();
  if (trimmed.startsWith(QStringLiteral("//"))) {
    return QStringLiteral("https:") + trimmed;
  }
  return trimmed;
}

// 取第一个捕获组
inline QString firstCapture(const QString &text, const QString &pattern,
                            int group = 1,
                            QRegularExpression::PatternOptions options =
                                QRegularExpression::CaseInsensitiveOption) {
  const QRegularExpression regex(pattern, options);
  const QRegularExpressionMatch match = regex.match(text);
  return match.hasMatch() ? match.captured(group) : QString();
}

// 取标签中的属性值，例如 attribute(tag, "title")
inline QString attribute(const QString &tag, const QString &name) {
  const QRegularExpression regex(
      QStringLiteral("\\b%1\\s*=\\s*[\"']([^\"']*)[\"']").arg(name),
      QRegularExpression::CaseInsensitiveOption);
  return firstCapture(tag, regex.pattern());
}

// 找到包含 marker 的 <div ...> 容器，返回其内部 HTML（按 div 深度配对）
inline QString containerInnerHtml(const QString &html, int markerPos) {
  if (markerPos < 0) {
    return QString();
  }
  const int openPos = html.lastIndexOf(QStringLiteral("<div"), markerPos);
  if (openPos < 0) {
    return QString();
  }
  int contentStart = html.indexOf(QLatin1Char('>'), openPos);
  if (contentStart < 0) {
    return QString();
  }
  ++contentStart;

  static const QRegularExpression divOpen(
      QStringLiteral("<div\\b[^>]*>"),
      QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression divClose(
      QStringLiteral("</div\\s*>"), QRegularExpression::CaseInsensitiveOption);
  int depth = 0;
  int pos = contentStart;
  while (pos < html.size()) {
    const QRegularExpressionMatch openMatch = divOpen.match(html, pos);
    const QRegularExpressionMatch closeMatch = divClose.match(html, pos);
    const int openIndex =
        openMatch.hasMatch() ? openMatch.capturedStart() : INT_MAX;
    const int closeIndex =
        closeMatch.hasMatch() ? closeMatch.capturedStart() : INT_MAX;
    if (openIndex == INT_MAX && closeIndex == INT_MAX) {
      break;
    }
    if (openIndex < closeIndex) {
      ++depth;
      pos = openMatch.capturedEnd();
    } else {
      if (depth == 0) {
        return html.mid(contentStart, closeIndex - contentStart);
      }
      --depth;
      pos = closeMatch.capturedEnd();
    }
  }
  return html.mid(contentStart);
}

// 依次取出所有匹配 pattern 的某个捕获组
inline QStringList captures(const QString &text, const QString &pattern,
                            int group = 1) {
  QStringList list;
  const QRegularExpression regex(pattern,
                                 QRegularExpression::CaseInsensitiveOption);
  QRegularExpressionMatchIterator it = regex.globalMatch(text);
  while (it.hasNext()) {
    const QString value = it.next().captured(group);
    if (!value.isEmpty() && !list.contains(value)) {
      list.append(value);
    }
  }
  return list;
}

// 用分隔符连接非空项
inline QString joinNonEmpty(const QStringList &list, const QString &sep) {
  QStringList filtered;
  for (const QString &item : list) {
    const QString trimmed = item.trimmed();
    if (!trimmed.isEmpty()) {
      filtered.append(trimmed);
    }
  }
  return filtered.join(sep);
}

// 清洗刮削得到的描述 HTML：去脚本/样式与颜色样式、压缩标签间空白、
// 协议相对地址补 https，便于在详情页直接渲染
inline QString sanitizeDescriptionHtml(QString html) {
  static const QRegularExpression scriptStyle(
      QStringLiteral("<(script|style)\\b[^>]*>.*?</\\1\\s*>"),
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::DotMatchesEverythingOption);
  html.remove(scriptStyle);

  static const QRegularExpression styleDouble(
      QStringLiteral("\\sstyle\\s*=\\s*\"[^\"]*\""),
      QRegularExpression::CaseInsensitiveOption);
  html.remove(styleDouble);
  static const QRegularExpression styleSingle(
      QStringLiteral("\\sstyle\\s*=\\s*'[^']*'"),
      QRegularExpression::CaseInsensitiveOption);
  html.remove(styleSingle);
  static const QRegularExpression colorAttr(
      QStringLiteral("\\scolor\\s*=\\s*[\"'][^\"']*[\"']"),
      QRegularExpression::CaseInsensitiveOption);
  html.remove(colorAttr);

  static const QRegularExpression emptyFont(
      QStringLiteral("<font\\s*>([^<]*)</font\\s*>"),
      QRegularExpression::CaseInsensitiveOption);
  html.replace(emptyFont, QStringLiteral("\\1"));

  static const QRegularExpression gap(QStringLiteral(">\\s+<"));
  html.replace(gap, QStringLiteral("><"));

  static const QRegularExpression proto(
      QStringLiteral("(src|href)=\"//"),
      QRegularExpression::CaseInsensitiveOption);
  html.replace(proto, QStringLiteral("\\1=\"https://"));

  return html.trimmed();
}

// ---- 描述清洗（原 utils/description，刮削结果与详情页展示共用） ----

namespace details {

// 块级标签：出现时说明内容已有段落结构，换行处理方式与纯文本不同
inline const QRegularExpression &blockTagRegex() {
  static const QRegularExpression regex(
      QStringLiteral(
          "<\\s*(p|div|ul|ol|li|pre|table|tr|td|h[1-6]|blockquote)\\b"),
      QRegularExpression::CaseInsensitiveOption);
  return regex;
}

// 内容清洗：把富文本整理成适合 QTextDocument 显示的 HTML。
// 顺序：去注释/脚本/iframe/模板 → 去图片链接 → 去表现属性 → 剥布局容器 →
// 去空标签 → 处理换行 → 合并连续 <br> → 去块首尾多余 <br>（详见 ai/data.md）。
inline QString normalizeHtml(const QString &html) {
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

  // template 区块：SSR 里是未渲染分支的模板，留着会重复或泄漏占位内容
  static const QRegularExpression templateBlock(
      QStringLiteral("<template\\b[^>]*>.*?</template\\s*>"),
      QRegularExpression::CaseInsensitiveOption |
          QRegularExpression::DotMatchesEverythingOption);
  text.remove(templateBlock);

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

  // 表现属性：class/id/data-*/aria-* 等对显示没有意义，页面的
  // 组件标记（如 data-v-*）留在介绍里只会干扰渲染。
  // 只改标签本身，避免把正文里出现的同名单词一并删掉
  static const QRegularExpression tagRe(
      QStringLiteral("<[a-zA-Z][^>]*>"),
      QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression presentationalAttr(
      QStringLiteral("\\s(?:class|id|role|dir|lang|data-[\\w-]+|aria-[\\w-]+)"
                     "(?:\\s*=\\s*(?:\"[^\"]*\"|'[^']*'|[^\\s>\"']+))?"),
      QRegularExpression::CaseInsensitiveOption);
  QString cleanedTags;
  int tagPos = 0;
  QRegularExpressionMatchIterator tagIt = tagRe.globalMatch(text);
  while (tagIt.hasNext()) {
    const QRegularExpressionMatch tag = tagIt.next();
    cleanedTags += text.mid(tagPos, tag.capturedStart() - tagPos);
    QString tagText = tag.captured(0);
    tagText.remove(presentationalAttr);
    cleanedTags += tagText;
    tagPos = tag.capturedEnd();
  }
  cleanedTags += text.mid(tagPos);
  text = cleanedTags;

  // 剥离纯布局用的容器外壳，块容器结束时补一个 <br> 保住段落边界
  static const QRegularExpression layoutCloseTag(
      QStringLiteral("</(?:div|section|article|main|aside|header|footer|nav|"
                     "figure|figcaption|picture)\\s*>"),
      QRegularExpression::CaseInsensitiveOption);
  text.replace(layoutCloseTag, QStringLiteral("<br>"));
  static const QRegularExpression layoutOpenTag(
      QStringLiteral("<(?:div|section|article|main|aside|header|footer|nav|"
                     "figure|figcaption|picture|source)\\b[^>]*>"),
      QRegularExpression::CaseInsensitiveOption);
  text.remove(layoutOpenTag);

  // 删除空标签（如只剩装饰的空 section/span/div），嵌套的经多轮清理
  static const QRegularExpression emptyTag(
      QStringLiteral("<([a-zA-Z][\\w-]*)\\b[^>]*>\\s*</\\1\\s*>"),
      QRegularExpression::CaseInsensitiveOption);
  for (int round = 0; round < 3 && text.contains(emptyTag); ++round) {
    text.remove(emptyTag);
  }

  // 删除空块（仅含空白或 <br> 的 p、li）
  static const QRegularExpression emptyBlock(
      QStringLiteral("<(p|li)\\b[^>]*>(?:\\s*<br\\s*/?>)*\\s*</\\1\\s*>"),
      QRegularExpression::CaseInsensitiveOption);
  text.remove(emptyBlock);

  // 区块标题（如 DLsite 的“作品内容”）：只是页面分区名，不是介绍正文
  static const QRegularExpression sectionHeading(
      QStringLiteral("<h[1-6]\\b[^>]*>\\s*(?:作品内容|作品紹介|作品情報)"
                     "\\s*</h[1-6]\\s*>"),
      QRegularExpression::CaseInsensitiveOption);
  text.remove(sectionHeading);

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

  // 统一 <br> 写法，避免 <br /> 与 <br> 混杂
  static const QRegularExpression brTag(
      QStringLiteral("<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption);
  text.replace(brTag, QStringLiteral("<br>"));

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
inline QString paragraphsHtml(const QString &html) {
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

} // namespace details

// 介绍里是否含块级标签（p/div/ul/ol/li/pre/table/h1-6/blockquote）
inline bool hasBlockLevelTags(const QString &html) {
  return details::blockTagRegex().match(html).hasMatch();
}

// 把游戏介绍（HTML 或纯文本）清洗、段落化为统一的可读 HTML。
// 详情页显示与入库保存共用，保证落库内容与显示一致
inline QString optimizeDescription(const QString &html) {
  return details::paragraphsHtml(details::normalizeHtml(html));
}

} // namespace scraper_utils
} // namespace GManager

#endif // !SCRAPER_UTILS_H
