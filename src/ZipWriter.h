#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

// Escritor de ZIP mínimo, método "stored" (sem compressão). Suficiente para
// empacotar arquivos que já são comprimidos por dentro (ODT/EPUB/DOCX são zips),
// onde recomprimir renderia pouco. Nomes de arquivo em UTF-8 (bit 11 do flag),
// então acentos no título dos capítulos/documentos saem corretos.
class ZipWriter {
public:
    // path usa "/" como separador de pastas. data é o conteúdo bruto do arquivo.
    // compress=true usa deflate (texto/HTML encolhe muito); false = stored
    // (obrigatório p/ o "mimetype" do EPUB e bom p/ dados já comprimidos).
    void addFile(const QString& path, const QByteArray& data, bool compress = true);

    // Monta e devolve o arquivo .zip completo.
    QByteArray finish();

private:
    struct Entry {
        QByteArray name;   // UTF-8
        quint32 crc = 0;
        quint32 compSize = 0;
        quint32 uncompSize = 0;
        quint16 method = 0;
        quint32 offset = 0;
    };
    QByteArray m_local;        // local headers + dados, na ordem de inserção
    QList<Entry> m_entries;
};
