#include "MainWindow.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>

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
    QApplication app(argc, argv);
    
    // Устанавливаем иконку приложения
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString logoPath = resolveAssetPath(QStringLiteral("logo.png"));
    if (!logoPath.isEmpty()) {
        app.setWindowIcon(QIcon(logoPath));
    }

    // Добавляем путь к плагинам Qt
    app.addLibraryPath(app.applicationDirPath() + "/plugins");

    // Try to load JetBrainsMono from local font/ directory.
    QString appFontFamily;
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("font/JetBrainsMono-Regular.ttf")),
        QDir(appDir).filePath(QStringLiteral("../font/JetBrainsMono-Regular.ttf")),
        QDir(appDir).filePath(QStringLiteral("../../font/JetBrainsMono-Regular.ttf")),
        QStringLiteral("font/JetBrainsMono-Regular.ttf")};

    QString fontPath;
    for (const QString &candidate : candidates) {
        if (QFile::exists(candidate)) {
            fontPath = candidate;
            break;
        }
    }

    if (!fontPath.isEmpty()) {
        const int id = QFontDatabase::addApplicationFont(fontPath);
        if (id >= 0) {
            const QStringList families = QFontDatabase::applicationFontFamilies(id);
            if (!families.isEmpty()) {
                appFontFamily = families.first();
            }
        }
    }

    if (!appFontFamily.isEmpty()) {
        app.setFont(QFont(appFontFamily, 9));
    } else {
        app.setFont(QFont(QStringLiteral("Segoe UI"), 9));
    }

    QString backendPath = QStringLiteral("orderbook_backend.exe");
    QString symbol = QStringLiteral("BIOUSDT");
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
