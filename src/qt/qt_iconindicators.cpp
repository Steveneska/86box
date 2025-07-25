#include <QSize>
#include <QPainter>
#include "qt_iconindicators.hpp"

QIcon
getIndicatorIcon(IconIndicator indicator)
{
    switch (indicator) {
        case Active:
            return QIcon(":/settings/qt/icons/active.ico");
        case WriteActive:
            return QIcon(":/settings/qt/icons/write_active.ico");
        case Disabled:
            return QIcon(":/settings/qt/icons/disabled.ico");
        case WriteProtected:
            return QIcon(":/settings/qt/icons/write_protected.ico");
        default:
            return QIcon();
    }
}

QPixmap
getIconWithIndicator(const QIcon &icon, const QSize &size, QIcon::Mode iconMode, IconIndicator indicator)
{
    auto iconPixmap = icon.pixmap(size, iconMode);

    if (indicator == None)
        return iconPixmap;

    auto painter = QPainter(&iconPixmap);
    auto indicatorPixmap = getIndicatorIcon((indicator == ReadWriteActive || indicator == WriteProtectedActive) ? Active : indicator).pixmap(size);

    painter.drawPixmap(0, 0, indicatorPixmap);
    if (indicator == ReadWriteActive || indicator == WriteProtectedActive) {
        auto writeIndicatorPixmap = getIndicatorIcon(indicator == WriteProtectedActive ? WriteProtected : WriteActive).pixmap(size);
        painter.drawPixmap(0, 0, writeIndicatorPixmap);
    }
    painter.end();

    return iconPixmap;
}