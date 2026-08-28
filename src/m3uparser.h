#pragma once

#include "channel.h"

#include <QByteArray>

namespace M3uParser {
ChannelList parse(const QByteArray &data);
}
