#include "rawio.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QQueue>
#include <QSet>
#include <QTransform>
#include <optional>

namespace {

quint16 u16(const char *p, bool le) {
    const auto *u = reinterpret_cast<const unsigned char *>(p);
    return le ? quint16(u[0] | (u[1] << 8)) : quint16((u[0] << 8) | u[1]);
}

quint32 u32(const char *p, bool le) {
    const auto *u = reinterpret_cast<const unsigned char *>(p);
    return le ? quint32(u[0] | (u[1] << 8) | (u[2] << 16) | (u[3] << 24))
              : quint32((u[0] << 24) | (u[1] << 16) | (u[2] << 8) | u[3]);
}

int scalar(int typ, quint32 count, quint32 inlinev, bool le) {
    if (count != 1)
        return int(inlinev);
    if (typ == 3)
        return le ? int(inlinev & 0xFFFF) : int((inlinev >> 16) & 0xFFFF);
    return int(inlinev);
}

struct JpegCand {
    int length = 0;
    qint64 off = 0;
    int orient = 1;
};

QVector<JpegCand> collectJpegs(QFile &f, bool le) {
    f.seek(0);
    const QByteArray head = f.read(8);
    if (head.size() < 8)
        return {};
    if (!(head.startsWith("II") || head.startsWith("MM")) || u16(head.constData() + 2, le) != 42)
        return {};
    QSet<qint64> seen;
    QVector<JpegCand> found;
    QQueue<qint64> queue;
    queue.enqueue(qint64(u32(head.constData() + 4, le)));
    while (!queue.isEmpty()) {
        const qint64 off = queue.dequeue();
        if (seen.contains(off) || off == 0)
            continue;
        seen.insert(off);
        f.seek(off);
        const QByteArray nbuf = f.read(2);
        if (nbuf.size() < 2)
            continue;
        const int n = u16(nbuf.constData(), le);
        const QByteArray block = f.read(n * 12 + 4);
        if (block.size() < n * 12 + 4)
            continue;
        std::optional<int> joff;
        std::optional<int> jlen;
        int orient = 1;
        for (int i = 0; i < n; ++i) {
            const int e = i * 12;
            const int tag = u16(block.constData() + e, le);
            const int typ = u16(block.constData() + e + 2, le);
            const quint32 count = u32(block.constData() + e + 4, le);
            const quint32 inlinev = u32(block.constData() + e + 8, le);
            if (tag == 274) {
                orient = scalar(typ, count, inlinev, le);
            } else if (tag == 513) {
                joff = scalar(typ, count, inlinev, le);
            } else if (tag == 514) {
                jlen = scalar(typ, count, inlinev, le);
            } else if (tag == 330) {
                if (count == 1) {
                    queue.enqueue(scalar(typ, count, inlinev, le));
                } else {
                    f.seek(qint64(inlinev));
                    const QByteArray raw = f.read(qint64(4 * count));
                    for (quint32 j = 0; j < count; ++j) {
                        if (int(j * 4 + 4) <= raw.size())
                            queue.enqueue(qint64(u32(raw.constData() + j * 4, le)));
                    }
                }
            }
        }
        const quint32 nxt = u32(block.constData() + n * 12, le);
        if (nxt)
            queue.enqueue(qint64(nxt));
        if (joff && jlen && *jlen >= 128)
            found.append({*jlen, qint64(*joff), orient});
    }
    return found;
}

QVector<JpegCand> jpegScan(QFile &f, int limit = 8000000) {
    f.seek(0);
    const QByteArray data = f.read(limit);
    QVector<JpegCand> found;
    int i = 0;
    while (true) {
        const int start = data.indexOf("\xff\xd8\xff", i);
        if (start < 0)
            break;
        const int end = data.indexOf("\xff\xd9", start + 3);
        if (end < 0)
            break;
        const int length = end + 2 - start;
        if (length >= 2000)
            found.append({length, qint64(start), 1});
        i = start + 3;
    }
    return found;
}

QImage applyOrient(QImage im, int orient) {
    if (im.isNull())
        return im;
    QTransform t;
    if (orient == 3)
        t.rotate(180);
    else if (orient == 6)
        t.rotate(90);
    else if (orient == 8)
        t.rotate(-90);
    else
        return im;
    return im.transformed(t, Qt::FastTransformation);
}

QImage imageFromJpeg(const QByteArray &data, int orient, int maxSide) {
    QByteArray bytes = data;
    QBuffer buf(&bytes);
    buf.open(QIODevice::ReadOnly);
    QImageReader r(&buf);
    r.setAutoTransform(true);
    if (maxSide > 0) {
        QSize s = r.size();
        if (s.isValid() && qMax(s.width(), s.height()) > maxSide)
            r.setScaledSize(s.scaled(maxSide, maxSide, Qt::KeepAspectRatio));
    }
    return applyOrient(r.read(), orient);
}

std::optional<QPair<QByteArray, int>> extractJpeg(const QString &path, bool thumb) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QByteArray sig = f.read(2);
    QVector<JpegCand> found;
    if (sig == "MM") {
        found = collectJpegs(f, false);
        if (found.isEmpty())
            found = jpegScan(f);
    } else if (sig == "II") {
        found = collectJpegs(f, true);
        if (found.isEmpty())
            found = jpegScan(f);
    } else {
        found = jpegScan(f);
    }
    if (found.isEmpty())
        return std::nullopt;
    std::sort(found.begin(), found.end(), [](const JpegCand &a, const JpegCand &b) {
        return a.length < b.length;
    });
    JpegCand pick = found.last();
    if (thumb) {
        for (const auto &c : found) {
            if (c.length >= 20000 && c.length <= 400000) {
                pick = c;
                break;
            }
        }
    }
    f.seek(pick.off);
    const QByteArray data = f.read(pick.length);
    if (data.size() < 128 || quint8(data[0]) != 0xff || quint8(data[1]) != 0xd8)
        return std::nullopt;
    return qMakePair(data, pick.orient);
}

} // namespace

bool isRaw(const QString &path) {
    return kRawExts.contains(QFileInfo(path).suffix().toLower());
}

QString savePathFor(const QString &path) {
    if (!isRaw(path))
        return path;
    QFileInfo fi(path);
    return fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".jpg");
}

QImage loadRaw(const QString &path, int maxSide) {
    const bool thumb = maxSide > 0 && maxSide <= 512;
    const auto got = extractJpeg(path, thumb);
    if (!got)
        return {};
    return imageFromJpeg(got->first, got->second, maxSide);
}
