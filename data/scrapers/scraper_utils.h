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

} // namespace scraper_utils
} // namespace GManager

#endif // !SCRAPER_UTILS_H
