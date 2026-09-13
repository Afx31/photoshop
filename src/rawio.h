#pragma once

#include <QImage>
#include <QSet>
#include <QString>

inline const QSet<QString> kRawExts = {
    QStringLiteral("arw"), QStringLiteral("cr2"), QStringLiteral("cr3"), QStringLiteral("dng"),
    QStringLiteral("nef"), QStringLiteral("nrw"), QStringLiteral("orf"), QStringLiteral("pef"),
    QStringLiteral("raf"), QStringLiteral("rw2"), QStringLiteral("srw"), QStringLiteral("raw"),
};

bool isRaw(const QString &path);
QString savePathFor(const QString &path);
QImage loadRaw(const QString &path, int maxSide = 0);
