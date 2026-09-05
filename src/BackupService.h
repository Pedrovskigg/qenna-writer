#pragma once

#include <QString>

// Backup completo de projeto (zip da pasta inteira) — feature que existia no
// Mira 1 (via PowerShell Compress-Archive) e nunca foi portada pro Qenna.
// Reimplementada aqui em cima do ZipWriter que o app já usa pro EPUB, sem
// depender de shell externo.
//
// Nasceu do incidente de 2026-09-02/03 (ver memória "qenna-trash-bin-decision"
// fora do repo): a Lixeira protege contra exclusão *dentro* do app, mas não
// contra o usuário apagar a pasta do projeto por fora (Explorador de
// Arquivos, formatação de disco, etc.) — só um backup em OUTRO lugar
// protege disso de verdade. Por isso a pasta de destino é sempre escolhida
// pelo usuário (idealmente fora de Documentos/do projeto — outro disco,
// nuvem sincronizada, pendrive) e nunca sugerida como subpasta do próprio
// projeto por padrão.
//
// Configuração é GLOBAL (QSettings, não por-projeto): pasta de destino, modo
// e intervalo valem pra qualquer projeto aberto. Estado de "quando foi a
// última vez" é por-projeto (chave derivada do caminho do projeto).
namespace BackupService {

enum class Mode {
    Off,        // recurso desligado
    Automatic,  // zipa sozinho a cada intervalo
    Reminder,   // não zipa sozinho — só avisa (toast) pra usuário fazer manual
};

struct Settings {
    Mode mode = Mode::Off;
    QString folder;       // pasta de destino dos .zip
    int intervalMinutes = 1440; // 1 dia
};

Settings loadSettings();
void saveSettings(const Settings& s);

// Timestamp (epoch ms) do último backup bem-sucedido desse projeto, ou 0 se
// nunca rodou. Timestamp do último AVISO de lembrete (independente de ter
// rodado backup), ou 0.
qint64 lastBackupAt(const QString& projectRoot);
qint64 lastReminderAt(const QString& projectRoot);
void markBackupDone(const QString& projectRoot, qint64 whenMs);
void markReminderShown(const QString& projectRoot, qint64 whenMs);

// Zipa a pasta inteira do projeto em destFolder/<nome do projeto> - backup
// <dd-MM-yyyy HHmm>.zip. Cria destFolder se não existir. Se destFolder for
// uma subpasta do próprio projectRoot, ela é excluída do conteúdo zipado
// (pra não incluir o zip dentro dele mesmo).
bool runBackupNow(const QString& projectRoot, const QString& destFolder,
                   QString* outZipPath = nullptr, QString* error = nullptr);

} // namespace BackupService
