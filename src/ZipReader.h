#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

// Leitor de ZIP mínimo, contraparte do ZipWriter. Suporta os dois métodos que
// o ZipWriter produz: "stored" (0) e "deflate" (8).
//
// Este leitor abre arquivos que vieram de outras pessoas — um .qtheme baixado
// da internet é entrada hostil por definição. Por isso ele valida tamanho,
// offset e CRC de cada entrada antes de devolver qualquer byte, e recusa
// arquivos absurdamente grandes (proteção contra "zip bomb"). Um zip corrompido
// ou malicioso faz open()/fileData() falharem, nunca estourarem.
class ZipReader {
public:
    // Teto por arquivo descomprimido (64 MB). Tema legítimo não chega perto —
    // é imagem de fundo e um punhado de JSON.
    static constexpr int kMaxEntrySize = 64 * 1024 * 1024;

    // Lê o índice central do zip. Não descomprime nada ainda.
    bool open(const QByteArray& zipData, QString* error = nullptr);

    QStringList fileNames() const { return m_order; }
    bool contains(const QString& path) const { return m_entries.contains(path); }

    // Descomprime e devolve uma entrada, validando o CRC. Em erro devolve
    // QByteArray() e põe *ok = false.
    QByteArray fileData(const QString& path, bool* ok = nullptr, QString* error = nullptr) const;

private:
    struct Entry {
        quint16 method = 0;
        quint32 crc = 0;
        quint32 compSize = 0;
        quint32 uncompSize = 0;
        quint32 localOffset = 0;
    };

    QByteArray m_data;
    QHash<QString, Entry> m_entries;
    QStringList m_order;
};
