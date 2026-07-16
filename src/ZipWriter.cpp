#include "ZipWriter.h"

namespace {

quint32 crc32(const QByteArray& data) {
    static quint32 table[256];
    static bool built = false;
    if (!built) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        built = true;
    }
    quint32 c = 0xFFFFFFFFu;
    const uchar* p = reinterpret_cast<const uchar*>(data.constData());
    const int n = data.size();
    for (int i = 0; i < n; ++i)
        c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

void putU16(QByteArray& b, quint16 v) {
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
}

void putU32(QByteArray& b, quint32 v) {
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
    b.append(char((v >> 16) & 0xFF));
    b.append(char((v >> 24) & 0xFF));
}

// Flag bit 11 = nome do arquivo em UTF-8.
constexpr quint16 kFlagUtf8 = 0x0800;
// Hora/data DOS fixa (1º jan 2020, 00:00) — válida e estável.
constexpr quint16 kDosTime = 0;
constexpr quint16 kDosDate = ((2020 - 1980) << 9) | (1 << 5) | 1;

} // namespace

void ZipWriter::addFile(const QString& path, const QByteArray& data, bool compress) {
    Entry e;
    e.name = path.toUtf8();
    e.crc = crc32(data);
    e.uncompSize = quint32(data.size());
    e.offset = quint32(m_local.size());

    // Deflate via qCompress: ele devolve [4 bytes tamanho][stream zlib][adler32].
    // O ZIP (method 8) quer o DEFLATE cru — removemos os 4 bytes do Qt + 2 do
    // cabeçalho zlib do início e os 4 do adler32 do fim.
    QByteArray payload = data;
    e.method = 0;
    if (compress && !data.isEmpty()) {
        const QByteArray z = qCompress(data, 9);
        if (z.size() > 10) {
            const QByteArray raw = z.mid(6, z.size() - 10);
            if (raw.size() < data.size()) { payload = raw; e.method = 8; }
        }
    }
    e.compSize = quint32(payload.size());

    // Local file header.
    putU32(m_local, 0x04034b50);
    putU16(m_local, 20);            // version needed
    putU16(m_local, kFlagUtf8);     // general purpose flag
    putU16(m_local, e.method);      // 0 = stored, 8 = deflate
    putU16(m_local, kDosTime);
    putU16(m_local, kDosDate);
    putU32(m_local, e.crc);
    putU32(m_local, e.compSize);
    putU32(m_local, e.uncompSize);
    putU16(m_local, quint16(e.name.size()));
    putU16(m_local, 0);             // extra field length
    m_local.append(e.name);
    m_local.append(payload);

    m_entries.append(e);
}

QByteArray ZipWriter::finish() {
    QByteArray central;
    for (const Entry& e : m_entries) {
        putU32(central, 0x02014b50);
        putU16(central, 20);            // version made by
        putU16(central, 20);            // version needed
        putU16(central, kFlagUtf8);
        putU16(central, e.method);      // 0 = stored, 8 = deflate
        putU16(central, kDosTime);
        putU16(central, kDosDate);
        putU32(central, e.crc);
        putU32(central, e.compSize);
        putU32(central, e.uncompSize);
        putU16(central, quint16(e.name.size()));
        putU16(central, 0);             // extra length
        putU16(central, 0);             // comment length
        putU16(central, 0);             // disk number start
        putU16(central, 0);             // internal attrs
        putU32(central, 0);             // external attrs
        putU32(central, e.offset);      // offset of local header
        central.append(e.name);
    }

    QByteArray out = m_local;
    const quint32 centralOffset = quint32(out.size());
    out.append(central);

    // End of central directory record.
    putU32(out, 0x06054b50);
    putU16(out, 0);                                  // disk number
    putU16(out, 0);                                  // disk with central dir
    putU16(out, quint16(m_entries.size()));          // entries on this disk
    putU16(out, quint16(m_entries.size()));          // total entries
    putU32(out, quint32(central.size()));            // central dir size
    putU32(out, centralOffset);                      // central dir offset
    putU16(out, 0);                                  // comment length
    return out;
}
