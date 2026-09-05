#include "SystemFolderGuard.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

namespace {

bool pathsEqual(const QString& a, const QString& b)
{
    if (a.isEmpty() || b.isEmpty()) return false;
    const QString ca = QDir::cleanPath(a);
    const QString cb = QDir::cleanPath(b);
#ifdef Q_OS_WIN
    // NTFS/paths do Windows não diferenciam maiúsculas/minúsculas.
    return ca.compare(cb, Qt::CaseInsensitive) == 0;
#else
    return ca == cb;
#endif
}

struct GuardedLocation {
    QStandardPaths::StandardLocation loc;
    const char* label; // QT_TR_NOOP — traduzido só na hora de exibir
};

const GuardedLocation kGuardedLocations[] = {
    { QStandardPaths::HomeLocation,      QT_TR_NOOP("a pasta pessoal do usuário") },
    { QStandardPaths::DocumentsLocation, QT_TR_NOOP("a pasta Documentos") },
    { QStandardPaths::DesktopLocation,   QT_TR_NOOP("a Área de Trabalho") },
    { QStandardPaths::DownloadLocation,  QT_TR_NOOP("a pasta Downloads") },
    { QStandardPaths::PicturesLocation,  QT_TR_NOOP("a pasta Imagens") },
    { QStandardPaths::MusicLocation,     QT_TR_NOOP("a pasta Música") },
    { QStandardPaths::MoviesLocation,    QT_TR_NOOP("a pasta Vídeos") },
    { QStandardPaths::AppDataLocation,   QT_TR_NOOP("a pasta de dados do próprio Qenna Writer") },
};

} // namespace

namespace SystemFolderGuard {

bool isProtected(const QString& path, QString* reasonOut)
{
    if (path.isEmpty()) return false;
    const QString clean = QDir::cleanPath(path);

    // Raiz de uma unidade inteira (C:\, D:\, / no Linux/macOS, etc.).
    if (QDir(clean).isRoot()) {
        if (reasonOut) *reasonOut = QCoreApplication::translate(
            "SystemFolderGuard", "a raiz de uma unidade de disco");
        return true;
    }

    for (const auto& g : kGuardedLocations) {
        for (const QString& loc : QStandardPaths::standardLocations(g.loc)) {
            if (pathsEqual(clean, loc)) {
                if (reasonOut) *reasonOut = QCoreApplication::translate("SystemFolderGuard", g.label);
                return true;
            }
        }
    }
    return false;
}

} // namespace SystemFolderGuard
