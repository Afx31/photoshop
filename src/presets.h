#pragma once

#include "adjustments.h"

#include <QString>
#include <QStringList>

namespace presets {

QStringList names();
void save(const QString &name, const Adjustments &adj);
bool apply(const QString &name, Adjustments &adj);
void remove(const QString &name);

}
