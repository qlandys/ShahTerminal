#include "MainWindow.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QGuiApplication>
#include <QSurfaceFormat>

static QString resolveAssetPath(const QString &relative)
{
    const QString rel = QDir::fromNativeSeparators(relative);
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList bases = {
        appDir,
        QDir(appDir).filePath(QStringLiteral("img")),
        QDir(appDir).filePath(QStringLiteral("img/icons")),
        QDir(appDir).filePath(QStringLiteral("img/icons/outline")),
        QDir(appDir).filePath(QStringLiteral("img/outline")),
        QDir(appDir).filePath(QStringLiteral("../img")),
        QDir(appDir).filePath(QStringLiteral("../img/icons")),
        QDir(appDir).filePath(QStringLiteral("../img/icons/outline")),
        QDir(appDir).filePath(QStringLiteral("../img/outline")),
        QDir(appDir).filePath(QStringLiteral("../../img")),
        QDir(appDir).filePath(QStringLiteral("../../img/icons")),
        QDir(appDir).filePath(QStringLiteral("../../img/icons/outline")),
        QDir(appDir).filePath(QStringLiteral("../../img/outline"))
    };
    for (const QString &base : bases) {
        const QString candidate = QDir(base).filePath(rel);
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }
    return QString();
}

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QCoreApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setProfile(QSurfaceFormat::NoProfile);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    fmt.setSamples(4);
    fmt.setSwapInterval(0);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString logoPath = resolveAssetPath(QStringLiteral("logo.png"));
    if (!logoPath.isEmpty()) {
        app.setWindowIcon(QIcon(logoPath));
    }

    app.addLibraryPath(app.applicationDirPath() + "/plugins");

    QString appFontFamily;
    const QStringList fontDirs = {
        QDir(appDir).filePath(QStringLiteral("font")),
        QDir(appDir).filePath(QStringLiteral("../font")),
        QDir(appDir).filePath(QStringLiteral("../../font"))};
    const QStringList preferredFiles = {
        QStringLiteral("InterVariable.ttf"),
        QStringLiteral("Inter-Regular.ttf"),
        QStringLiteral("Inter_18pt-Regular.ttf")};
    for (const QString &dirPath : fontDirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;
        QStringList files;
        for (const QString &pf : preferredFiles) {
            if (dir.exists(pf)) files << pf;
        }
        files << dir.entryList(QStringList() << QStringLiteral("Inter*.ttf"), QDir::Files);
        for (const QString &file : files) {
            const int id = QFontDatabase::addApplicationFont(dir.filePath(file));
            if (id < 0) continue;
            const QStringList families = QFontDatabase::applicationFontFamilies(id);
            for (const QString &fam : families) {
                if (fam.startsWith(QStringLiteral("Inter"), Qt::CaseInsensitive)) {
                    appFontFamily = fam;
                    break;
                }
            }
            if (!appFontFamily.isEmpty()) break;
        }
        if (!appFontFamily.isEmpty()) break;
    }

    QFont appFont = appFontFamily.isEmpty() ? QFont(QStringLiteral("Segoe UI"), 10)
                                            : QFont(appFontFamily, 10, QFont::Normal);
    const auto strategy = static_cast<QFont::StyleStrategy>(QFont::PreferAntialias | QFont::PreferQuality);
    appFont.setStyleHint(QFont::SansSerif, strategy);
    appFont.setStyleStrategy(strategy);
    appFont.setHintingPreference(QFont::PreferNoHinting);
    appFont.setKerning(true);
    app.setFont(appFont);

    QString backendPath = QStringLiteral("orderbook_backend.exe");
    QString symbol;
    int levels = 500; // 500 per side => ~1000 levels total

    // very simple CLI parsing: --symbol XXX --levels N --backend-path PATH
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--symbol" && i + 1 < argc) {
            symbol = QString::fromLocal8Bit(argv[++i]);
        } else if (arg == "--levels" && i + 1 < argc) {
            levels = QString::fromLocal8Bit(argv[++i]).toInt();
        } else if (arg == "--backend-path" && i + 1 < argc) {
            backendPath = QString::fromLocal8Bit(argv[++i]);
        }
    }

    MainWindow win(backendPath, symbol, levels);
    win.show();
    return app.exec();
}
