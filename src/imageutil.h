#pragma once

#include "adjustments.h"

#include <QImage>
#include <QSet>
#include <QString>

inline const QSet<QString> kImageExts = {
    QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("webp"),
    QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("bmp"), QStringLiteral("heic"),
    QStringLiteral("heif"), QStringLiteral("jxl"),
};

QImage rotateImage(const QImage &im, int rotation);
QImage cropImage(const QImage &im, const std::optional<QRectF> &crop);
QImage scalePreview(const QImage &im, int maxSide = 2048);
QImage applyColor(const QImage &im, const Adjustments &adj);
QImage develop(const QImage &im, const Adjustments &adj, bool preview = false);
QImage loadImage(const QString &path, int maxSide = 0);
bool saveImage(const QImage &im, const QString &path);
