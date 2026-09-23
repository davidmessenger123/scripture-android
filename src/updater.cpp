#include "updater.h"

#include <QDesktopServices>
#include <QUrl>

namespace {
constexpr char kReleaseUrl[] = "https://github.com/davidmessenger123/scripture-windows/releases";
} // namespace

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
{
}

void UpdateChecker::open()
{
    QDesktopServices::openUrl(QUrl(QLatin1String(kReleaseUrl)));
}