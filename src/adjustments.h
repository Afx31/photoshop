#pragma once

#include <QRectF>
#include <QString>
#include <QStringList>
#include <cmath>
#include <optional>

inline const QStringList kColorKeys = {
    QStringLiteral("exposure"),
    QStringLiteral("contrast"),
    QStringLiteral("highlights"),
    QStringLiteral("shadows"),
    QStringLiteral("whites"),
    QStringLiteral("blacks"),
    QStringLiteral("temperature"),
    QStringLiteral("tint"),
    QStringLiteral("vibrance"),
    QStringLiteral("saturation"),
};

struct Adjustments {
    double exposure = 0;
    double contrast = 0;
    double highlights = 0;
    double shadows = 0;
    double whites = 0;
    double blacks = 0;
    double temperature = 0;
    double tint = 0;
    double vibrance = 0;
    double saturation = 0;
    int rotation = 0;
    std::optional<QRectF> crop;

    Adjustments copy() const { return *this; }

    double *field(const QString &key) {
        if (key == QLatin1String("exposure")) return &exposure;
        if (key == QLatin1String("contrast")) return &contrast;
        if (key == QLatin1String("highlights")) return &highlights;
        if (key == QLatin1String("shadows")) return &shadows;
        if (key == QLatin1String("whites")) return &whites;
        if (key == QLatin1String("blacks")) return &blacks;
        if (key == QLatin1String("temperature")) return &temperature;
        if (key == QLatin1String("tint")) return &tint;
        if (key == QLatin1String("vibrance")) return &vibrance;
        if (key == QLatin1String("saturation")) return &saturation;
        return nullptr;
    }

    double get(const QString &key) const {
        auto *self = const_cast<Adjustments *>(this);
        double *f = self->field(key);
        return f ? *f : 0;
    }

    void set(const QString &key, double value) {
        if (double *f = field(key))
            *f = value;
    }

    bool colorIsDefault() const {
        for (const auto &k : kColorKeys) {
            if (std::abs(get(k)) >= 1e-6)
                return false;
        }
        return true;
    }

    bool isDefault() const {
        return colorIsDefault() && (rotation % 360) == 0 && !crop.has_value();
    }

    void rotated(int delta) {
        int old = ((rotation % 360) + 360) % 360;
        rotation = (old + delta) % 360;
        if (!crop)
            return;
        double x = crop->x();
        double y = crop->y();
        double w = crop->width();
        double h = crop->height();
        int turns = ((delta / 90) % 4 + 4) % 4;
        for (int i = 0; i < turns; ++i) {
            double nx = 1.0 - y - h;
            double ny = x;
            double nw = h;
            double nh = w;
            x = nx;
            y = ny;
            w = nw;
            h = nh;
        }
        crop = QRectF(x, y, w, h);
    }
};

struct Photo {
    QString path;
    Adjustments adj;
    bool dirty = false;
    int width = 0;
    int height = 0;
};
