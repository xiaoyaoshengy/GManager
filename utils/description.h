#ifndef DESCRIPTION_H
#define DESCRIPTION_H

#include <QString>

namespace GManager {

// 把游戏介绍（HTML 或纯文本）清洗、段落化为统一的可读 HTML。
// 详情页显示与入库保存共用，保证落库内容与显示一致
QString optimizeDescription(const QString &html);

// 介绍里是否含块级标签（p/div/ul/ol/li/pre/table/h1-6/blockquote）
bool hasBlockLevelTags(const QString &html);

} // namespace GManager

#endif // !DESCRIPTION_H
