#include "imageutil.h"
#include "engine.h"
#include "rawio.h"

#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QTransform>
#include <cstring>
#include <libheif/heif.h>
#include <jxl/decode.h>
#include <jxl/resizable_parallel_runner.h>

namespace {

QImage loadHeif(const QString &path, int maxSide) {
    heif_context *ctx = heif_context_alloc();
    heif_error err = heif_context_read_from_file(ctx, path.toUtf8().constData(), nullptr);
    if (err.code != heif_error_Ok) {
        heif_context_free(ctx);
        return {};
    }
    heif_image_handle *handle = nullptr;
    err = heif_context_get_primary_image_handle(ctx, &handle);
    if (err.code != heif_error_Ok) {
        heif_context_free(ctx);
        return {};
    }
    heif_image *him = nullptr;
    err = heif_decode_image(handle, &him, heif_colorspace_RGB, heif_chroma_interleaved_RGB, nullptr);
    if (err.code != heif_error_Ok) {
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        return {};
    }
    int stride = 0;
    const uint8_t *data = heif_image_get_plane_readonly(him, heif_channel_interleaved, &stride);
    const int w = heif_image_get_width(him, heif_channel_interleaved);
    const int h = heif_image_get_height(him, heif_channel_interleaved);
    QImage im(w, h, QImage::Format_RGB888);
    if (!im.isNull() && data) {
        for (int y = 0; y < h; ++y)
            memcpy(im.scanLine(y), data + y * stride, size_t(w) * 3);
    }
    heif_image_release(him);
    heif_image_handle_release(handle);
    heif_context_free(ctx);
    if (maxSide > 0 && qMax(im.width(), im.height()) > maxSide)
        im = im.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return im;
}

QImage loadJxl(const QString &path, int maxSide) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QByteArray data = f.readAll();
    JxlDecoder *dec = JxlDecoderCreate(nullptr);
    if (!dec)
        return {};
    void *runner = JxlResizableParallelRunnerCreate(nullptr);
    JxlDecoderSetParallelRunner(dec, JxlResizableParallelRunner, runner);
    JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE);
    JxlDecoderSetInput(dec, reinterpret_cast<const uint8_t *>(data.constData()), size_t(data.size()));
    JxlDecoderCloseInput(dec);
    JxlBasicInfo info;
    memset(&info, 0, sizeof(info));
    QByteArray pixels;
    QImage im;
    bool ok = false;
    for (;;) {
        const JxlDecoderStatus st = JxlDecoderProcessInput(dec);
        if (st == JXL_DEC_ERROR || st == JXL_DEC_NEED_MORE_INPUT)
            break;
        if (st == JXL_DEC_BASIC_INFO) {
            JxlDecoderGetBasicInfo(dec, &info);
        } else if (st == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            JxlPixelFormat fmt{3, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
            size_t bufSize = 0;
            JxlDecoderImageOutBufferSize(dec, &fmt, &bufSize);
            pixels.resize(int(bufSize));
            JxlDecoderSetImageOutBuffer(dec, &fmt, pixels.data(), bufSize);
        } else if (st == JXL_DEC_FULL_IMAGE) {
            im = QImage(int(info.xsize), int(info.ysize), QImage::Format_RGB888);
            if (!im.isNull() && pixels.size() >= im.height() * im.bytesPerLine()) {
                const int row = int(info.xsize) * 3;
                for (int y = 0; y < im.height(); ++y)
                    memcpy(im.scanLine(y), pixels.constData() + y * row, size_t(row));
            }
        } else if (st == JXL_DEC_SUCCESS) {
            ok = !im.isNull();
            break;
        }
    }
    JxlDecoderDestroy(dec);
    JxlResizableParallelRunnerDestroy(runner);
    if (!ok)
        return {};
    if (maxSide > 0 && qMax(im.width(), im.height()) > maxSide)
        im = im.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return im;
}

QImage loadQt(const QString &path, int maxSide) {
    QImageReader r(path);
    r.setAutoTransform(true);
    if (maxSide > 0) {
        QSize s = r.size();
        if (s.isValid() && qMax(s.width(), s.height()) > maxSide)
            r.setScaledSize(s.scaled(maxSide, maxSide, Qt::KeepAspectRatio));
    }
    return r.read();
}

} // namespace

QImage rotateImage(const QImage &im, int rotation) {
    int r = ((rotation % 360) + 360) % 360;
    if (r == 0 || im.isNull())
        return im;
    QTransform t;
    t.rotate(r);
    return im.transformed(t, Qt::FastTransformation);
}

QImage cropImage(const QImage &im, const std::optional<QRectF> &crop) {
    if (!crop || im.isNull())
        return im;
    const int w = im.width();
    const int h = im.height();
    int x = qRound(crop->x() * w);
    int y = qRound(crop->y() * h);
    int cw = qRound(crop->width() * w);
    int ch = qRound(crop->height() * h);
    x = qBound(0, x, w - 1);
    y = qBound(0, y, h - 1);
    cw = qBound(1, cw, w - x);
    ch = qBound(1, ch, h - y);
    return im.copy(x, y, cw, ch);
}

QImage scalePreview(const QImage &im, int maxSide) {
    if (im.isNull())
        return im;
    const int side = qMax(im.width(), im.height());
    if (side <= maxSide)
        return im;
    return im.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QImage applyColor(const QImage &im, const Adjustments &adj) {
    if (im.isNull())
        return im;
    if (adj.colorIsDefault() && !im.hasAlphaChannel())
        return im;
    const QImage src = im.convertToFormat(QImage::Format_RGB888);
    QImage dst(src.size(), QImage::Format_RGB888);
    if (dst.isNull())
        return {};
    apply_adjustments(
        src.constBits(),
        dst.bits(),
        src.width(),
        src.height(),
        src.bytesPerLine(),
        3,
        dst.bytesPerLine(),
        float(adj.exposure),
        float(adj.contrast) / 100.f,
        float(adj.highlights) / 100.f,
        float(adj.shadows) / 100.f,
        float(adj.whites) / 100.f,
        float(adj.blacks) / 100.f,
        float(adj.temperature) / 100.f,
        float(adj.tint) / 100.f,
        float(adj.vibrance) / 100.f,
        float(adj.saturation) / 100.f);
    return dst;
}

QImage develop(const QImage &im, const Adjustments &adj, bool preview) {
    QImage out = rotateImage(im, adj.rotation);
    out = cropImage(out, adj.crop);
    if (preview)
        out = scalePreview(out);
    return applyColor(out, adj);
}

QImage loadImage(const QString &path, int maxSide) {
    const QString ext = QFileInfo(path).suffix().toLower();
    if (isRaw(path))
        return loadRaw(path, maxSide);
    if (ext == QLatin1String("heic") || ext == QLatin1String("heif"))
        return loadHeif(path, maxSide);
    if (ext == QLatin1String("jxl"))
        return loadJxl(path, maxSide);
    return loadQt(path, maxSide);
}

bool saveImage(const QImage &im, const QString &path) {
    const QString dest = savePathFor(path);
    const QString suffix = QFileInfo(dest).suffix().toLower();
    const char *fmt = "JPEG";
    int quality = 95;
    if (suffix == QLatin1String("png")) {
        fmt = "PNG";
        quality = 80;
    } else if (suffix == QLatin1String("webp")) {
        fmt = "WEBP";
        quality = 95;
    } else if (suffix == QLatin1String("tif") || suffix == QLatin1String("tiff")) {
        fmt = "TIFF";
        quality = -1;
    } else if (suffix == QLatin1String("bmp")) {
        fmt = "BMP";
        quality = -1;
    } else if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")) {
        fmt = "JPEG";
        quality = 95;
    }
    return im.save(dest, fmt, quality);
}
