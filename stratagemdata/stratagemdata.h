#ifndef STRATAGEMDATA_H
#define STRATAGEMDATA_H

#include <QString>
#include <QVector>
#include <Qt>
enum class StratagemType {
    Offensive,  // 红色战备（进攻）
    Supply,     // 蓝色战备（补给/装备）
    Defensive,  // 绿色战备（防御/工事）
    Utility     // 黄色战备（功能/特殊）
};
// 战备数据结构体
struct StratagemData {
    QString id;
    QString name;
    StratagemType type;            // 改为枚举类型
    QString iconPath;
    QString largeIconPath;
    QVector<Qt::Key> codeSequence;

    StratagemData() : type(StratagemType::Utility) {}
};

#endif // STRATAGEMDATA_H
