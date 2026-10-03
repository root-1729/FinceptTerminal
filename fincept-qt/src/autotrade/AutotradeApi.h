#pragma once
#include <QString>

// Autotrade integration (root-1729 fork). Lives entirely under src/autotrade/ so
// upstream merges only ever touch the few one-line hooks listed in autotrade.cmake.

namespace fincept::autotrade {

/// Base URL of the autotrade api-gateway, without a trailing slash. First match wins:
///   1. FINCEPT_AUTOTRADE_URL environment variable
///   2. AppConfig key "autotrade/api_url"
///   3. http://localhost:8000
QString api_base_url();

/// api_base_url() + path; path starts with '/'.
QString api_url(const QString& path);

} // namespace fincept::autotrade
