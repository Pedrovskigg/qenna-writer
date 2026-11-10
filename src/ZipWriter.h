#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

class QIODevice;

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

    // Com um dispositivo, cada arquivo vai direto pro disco em vez de acumular na
    // memória (projeto com muitas imagens passa de 1 GB). Chamar antes do addFile.
    void setDevice(QIODevice* device) { m_device = device; }
    bool failed() const { return m_failed; }

    // Monta e devolve o arquivo .zip completo. Com dispositivo, grava o final
    // nele e devolve vazio.
    QByteArray finish();

private:
    void flushToDevice(const QByteArray& bytes);
    QIODevice* m_device = nullptr;
    bool m_failed = false;
    quint32 m_flushed = 0;     // bytes já gravados no dispositivo

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
