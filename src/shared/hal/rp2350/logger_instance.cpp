#include "logger_instance.h"

RttSink g_log_sink;
Logger<RttSink> g_log(g_log_sink);
