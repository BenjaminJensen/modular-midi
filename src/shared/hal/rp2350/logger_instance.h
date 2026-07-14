#pragma once

#include "rtt_sink.h"
#include "shared/logger.h"

// Single global logger instance for this platform, so call sites in any
// task/file can log without threading a reference through every function.
extern RttSink g_log_sink;
extern Logger<RttSink> g_log;
