#pragma once

#include <QCoreApplication>
#include <QString>

// Pasta dos arquivos que o app lê do disco em runtime (fontes, imagens de
// tema, dicionários, sons, artes do Q). No Windows ela é a pasta do exe. No Mac
// fica em Contents/Resources do pacote .app: o codesign trata qualquer pasta
// dentro de Contents/MacOS como código, e a assinatura do app quebra.
inline QString appAssetsDir()
{
#ifdef Q_OS_MACOS
    return QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources");
#else
    return QCoreApplication::applicationDirPath();
#endif
}
