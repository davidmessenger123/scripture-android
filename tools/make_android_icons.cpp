// Render the Scripture app icon for Android, matching the Windows/macOS art.
//
// Faithful C++ port of scripture-windows/scripts/make_icon.py: a navy (#0d1b2a)
// rounded square with a gold (#f5c542) Latin cross. Emits:
//   - legacy launcher tiles at every density (mipmap-{mdpi..xxxhdpi})
//   - the adaptive-icon foreground (cross on a transparent field, kept inside
//     the 66dp safe zone) and a white monochrome variant for themed icons
//
// Build with the desktop Qt:
//   cmake -S tools -B tools/build -GNinja && cmake --build tools/build
// Then run from the repo root:  ./tools/build/make_android_icons <out-dir>

#include <QDir>
#include <QImageWriter>
#include <QPainter>
#include <QRectF>
#include <QString>
#include <QColor>

#include <cstdlib>
#include <iostream>

namespace {

void drawCrossCentered(QPainter &painter, double size, const QColor &color)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    const double r = size / 22;
    // Vertical beam (long lower arm) and horizontal beam, centered on the origin.
    painter.drawRoundedRect(QRectF(-0.07 * size, -0.35 * size, 0.14 * size, 0.62 * size), r, r);
    painter.drawRoundedRect(QRectF(-0.18 * size, -0.20 * size, 0.36 * size, 0.14 * size), r, r);
}

void drawCrossTile(QPainter &painter, double size)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#f5c542"));
    const double r = size / 22;
    painter.drawRoundedRect(QRectF(size * 0.43, size * 0.15, size * 0.14, size * 0.62), r, r);
    painter.drawRoundedRect(QRectF(size * 0.32, size * 0.30, size * 0.36, size * 0.14), r, r);
}

QImage renderTile(int size)
{
    QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#0d1b2a"));
    const double inset = size * 4.0 / 64.0;
    const double radius = size * 12.0 / 64.0;
    painter.drawRoundedRect(QRectF(inset, inset, size - 2 * inset, size - 2 * inset), radius, radius);
    drawCrossTile(painter, size);
    painter.end();
    return img;
}

QImage renderCrossOnly(int canvas, double crossSize, const QColor &color)
{
    QImage img(canvas, canvas, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(canvas / 2.0, canvas / 2.0);
    drawCrossCentered(painter, crossSize, color);
    painter.end();
    return img;
}

bool writePng(const QString &path, const QImage &img)
{
    QImageWriter writer(path, "png");
    if (!writer.write(img)) {
        std::cerr << "could not write " << path.toStdString()
                  << ": " << writer.errorString().toStdString() << "\n";
        return false;
    }
    std::cout << "wrote " << path.toStdString() << " (" << img.width() << "x"
              << img.height() << ")\n";
    return true;
}

bool writeAll(const QString &outRoot)
{
    // Legacy launcher tiles (pre-adaptive icons): exact desktop tile art.
    // Canvas sizes are 48/72/96/144/192 px for mdpi..xxxhdpi.
    const struct
    {
        const char *density;
        int size;
    } densities[] = {
        {"mipmap-mdpi", 48},   {"mipmap-hdpi", 72},   {"mipmap-xhdpi", 96},
        {"mipmap-xxhdpi", 144}, {"mipmap-xxxhdpi", 192},
    };
    for (const auto &d : densities) {
        const QDir dir = QDir(outRoot).filePath(QLatin1String(d.density));
        if (!dir.mkpath("."))
            return false;
        const QImage tile = renderTile(d.size);
        if (!writePng(dir.filePath("ic_launcher.png"), tile))
            return false;
        if (!writePng(dir.filePath("ic_launcher_round.png"), tile))
            return false;
    }

    // Adaptive icon foreground: 108dp canvas rendered at 4x (xxxhdpi) = 432px.
    // The gold cross fills ~82% of the canvas so it stays inside the 66/108
    // safe zone when the launcher masks and scales it.
    constexpr int kCanvas = 432;
    const double crossSize = kCanvas * 0.82;
    const QDir drawable = QDir(outRoot).filePath("drawable");
    if (!drawable.mkpath("."))
        return false;
    if (!writePng(drawable.filePath("ic_launcher_foreground.png"), renderCrossOnly(kCanvas, crossSize, QColor("#f5c542"))))
        return false;
    if (!writePng(drawable.filePath("ic_launcher_monochrome.png"), renderCrossOnly(kCanvas, crossSize, QColor("#ffffff"))))
        return false;

    return true;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "usage: make_android_icons <output-res-dir>\n";
        return 2;
    }
    return writeAll(QString::fromLocal8Bit(argv[1])) ? 0 : 1;
}