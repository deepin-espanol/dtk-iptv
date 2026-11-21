#pragma once

#include "channel.h"

#include <QByteArray>

#include <functional>

namespace M3uParser {
// Entries whose URL makes `skip` return true are left out.
ChannelList parse(const QByteArray &data, const std::function<bool(const QString &)> &skip = {});
}
