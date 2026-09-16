#include "ZipReader.h"

#include <QCoreApplication>

#include <zlib.h>

namespace {

// Leitura little-endian com checagem de limite: qualquer offset fora do buffer
// devolve 0 e marca falha, em vez de ler memória alheia.
quint16 readU16(const QByteArray& b, int off, bool* ok)
{
    if (off < 0 || off + 2 > b.size()) { *ok = false; return 0; }
    const uchar* p = reinterpret_cast<const uchar*>(b.constData()) + off;
    return static_cast<quint16>(p[0] | (p[1] << 8));
}

quint32 readU32(const QByteArray& b, int off, bool* ok)
{
    if (off < 0 || off + 4 > b.size()) { *ok = false; return 0; }
    const uchar* p = reinterpret_cast<const uchar*>(b.constData()) + off;
    return static_cast<quint32>(p[0]) | (static_cast<quint32>(p[1]) << 8)
         | (static_cast<quint32>(p[2]) << 16) | (static_cast<quint32>(p[3]) << 24);
}

// Descomprime um bloco de deflate cru (sem cabeçalho zlib), que é o que vive
// dentro de um zip. qUncompress não serve aqui: ele espera o envelope zlib com
// adler32 no fim, que o formato zip não guarda.
QByteArray inflateRaw(const QByteArray& in, quint32 expectedSize, bool* ok)
{
    *ok = false;
    if (expectedSize == 0) { *ok = true; return QByteArray(); }

    QByteArray out;
    out.resize(static_cast<int>(expectedSize));

    z_stream strm = {};
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.constData()));
    strm.avail_in = static_cast<uInt>(in.size());
    strm.next_out = reinterpret_cast<Bytef*>(out.data());
    strm.avail_out = static_cast<uInt>(expectedSize);

    // windowBits negativo = deflate cru, sem esperar cabeçalho.
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) return QByteArray();

    const int rc = inflate(&strm, Z_FINISH);
    const uInt produced = static_cast<uInt>(expectedSize) - strm.avail_out;
    inflateEnd(&strm);

    // Exigimos exatamente o tamanho anunciado no índice: se o stream não
    // terminar certinho, o arquivo está corrompido ou mentindo sobre si mesmo.
    if (rc != Z_STREAM_END || produced != expectedSize) return QByteArray();

    *ok = true;
    return out;
}

} // namespace

bool ZipReader::open(const QByteArray& zipData, QString* error)
{
    m_data.clear();
    m_entries.clear();
    m_order.clear();

    const auto fail = [&](const QString& msg) {
        if (error) *error = msg;
        return false;
    };

    // O "End of Central Directory" fica no fim, mas pode ter até 64 KB de
    // comentário depois dele, então varremos de trás pra frente atrás da
    // assinatura em vez de assumir posição fixa.
    constexpr int kEocdMinSize = 22;
    if (zipData.size() < kEocdMinSize)
        return fail(QCoreApplication::translate("ZipReader", "O arquivo não é um ZIP válido (pequeno demais)."));

    int eocd = -1;
    const int searchFloor = qMax(0, zipData.size() - (65535 + kEocdMinSize));
    for (int i = zipData.size() - kEocdMinSize; i >= searchFloor; --i) {
        bool ok = true;
        if (readU32(zipData, i, &ok) == 0x06054b50 && ok) { eocd = i; break; }
    }
    if (eocd < 0)
        return fail(QCoreApplication::translate("ZipReader", "O arquivo não é um ZIP válido (índice não encontrado)."));

    bool ok = true;
    const quint16 count   = readU16(zipData, eocd + 10, &ok);
    const quint32 cdSize  = readU32(zipData, eocd + 12, &ok);
    const quint32 cdStart = readU32(zipData, eocd + 16, &ok);
    if (!ok || static_cast<qint64>(cdStart) + cdSize > zipData.size())
        return fail(QCoreApplication::translate("ZipReader", "O índice do ZIP aponta pra fora do arquivo."));

    int pos = static_cast<int>(cdStart);
    for (int i = 0; i < count; ++i) {
        ok = true;
        if (readU32(zipData, pos, &ok) != 0x02014b50 || !ok)
            return fail(QCoreApplication::translate("ZipReader", "O índice do ZIP está corrompido."));

        Entry e;
        e.method                 = readU16(zipData, pos + 10, &ok);
        e.crc                    = readU32(zipData, pos + 16, &ok);
        e.compSize               = readU32(zipData, pos + 20, &ok);
        e.uncompSize             = readU32(zipData, pos + 24, &ok);
        const quint16 nameLen    = readU16(zipData, pos + 28, &ok);
        const quint16 extraLen   = readU16(zipData, pos + 30, &ok);
        const quint16 commentLen = readU16(zipData, pos + 32, &ok);
        e.localOffset            = readU32(zipData, pos + 42, &ok);
        if (!ok) return fail(QCoreApplication::translate("ZipReader", "O índice do ZIP está corrompido."));

        if (pos + 46 + nameLen > zipData.size())
            return fail(QCoreApplication::translate("ZipReader", "O índice do ZIP está corrompido."));
        const QString name = QString::fromUtf8(zipData.constData() + pos + 46, nameLen);

        if (e.uncompSize > kMaxEntrySize || e.compSize > kMaxEntrySize)
            return fail(QCoreApplication::translate("ZipReader", "O arquivo tem um item grande demais (acima de 64 MB)."));

        // Pastas entram no índice como entradas de tamanho zero terminadas em
        // "/". Não interessam aqui: o .qtheme é plano.
        if (!name.endsWith(QLatin1Char('/'))) {
            if (!m_entries.contains(name)) m_order.append(name);
            m_entries.insert(name, e);
        }

        pos += 46 + nameLen + extraLen + commentLen;
    }

    m_data = zipData;
    return true;
}

QByteArray ZipReader::fileData(const QString& path, bool* ok, QString* error) const
{
    const auto fail = [&](const QString& msg) {
        if (ok) *ok = false;
        if (error) *error = msg;
        return QByteArray();
    };

    const auto it = m_entries.constFind(path);
    if (it == m_entries.constEnd())
        return fail(QCoreApplication::translate("ZipReader", "Item não encontrado no arquivo: %1").arg(path));

    const Entry& e = *it;

    // O cabeçalho local repete nome e "extra", com tamanhos que podem diferir
    // dos do índice — os dados começam depois deles, então relemos daqui.
    bool hdrOk = true;
    if (readU32(m_data, static_cast<int>(e.localOffset), &hdrOk) != 0x04034b50 || !hdrOk)
        return fail(QCoreApplication::translate("ZipReader", "O arquivo está corrompido."));
    const quint16 nameLen  = readU16(m_data, static_cast<int>(e.localOffset) + 26, &hdrOk);
    const quint16 extraLen = readU16(m_data, static_cast<int>(e.localOffset) + 28, &hdrOk);
    if (!hdrOk) return fail(QCoreApplication::translate("ZipReader", "O arquivo está corrompido."));

    const qint64 dataStart = static_cast<qint64>(e.localOffset) + 30 + nameLen + extraLen;
    if (dataStart + e.compSize > m_data.size())
        return fail(QCoreApplication::translate("ZipReader", "O arquivo está corrompido."));

    const QByteArray raw = m_data.mid(static_cast<int>(dataStart), static_cast<int>(e.compSize));

    QByteArray out;
    if (e.method == 0) {
        if (e.compSize != e.uncompSize)
            return fail(QCoreApplication::translate("ZipReader", "O arquivo está corrompido."));
        out = raw;
    } else if (e.method == 8) {
        bool infOk = false;
        out = inflateRaw(raw, e.uncompSize, &infOk);
        if (!infOk)
            return fail(QCoreApplication::translate("ZipReader", "Não foi possível descompactar o arquivo."));
    } else {
        return fail(QCoreApplication::translate("ZipReader", "O arquivo usa um método de compactação não suportado."));
    }

    // CRC do índice contra CRC do que saiu: pega corrupção silenciosa que
    // passaria batida (um byte trocado no meio de uma imagem, por exemplo).
    const quint32 actual = static_cast<quint32>(
        crc32(0, reinterpret_cast<const Bytef*>(out.constData()), static_cast<uInt>(out.size())));
    if (actual != e.crc)
        return fail(QCoreApplication::translate("ZipReader", "O arquivo está corrompido (verificação falhou)."));

    if (ok) *ok = true;
    return out;
}
