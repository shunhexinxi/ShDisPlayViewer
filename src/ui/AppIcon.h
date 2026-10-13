#pragma once
#include <QIcon>
#include <QSize>

namespace Zqxxxt {
inline QIcon applicationIcon()
{
    static const QIcon icon = [] {
        QIcon value;
        for (int size : {16, 24, 32, 48, 64, 128, 256, 512})
            value.addFile(QStringLiteral(":/app/icons/%1.png").arg(size), QSize(size, size));
        return value;
    }();
    return icon;
}
}
