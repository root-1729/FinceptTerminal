#include "autotrade/AutotradeApi.h"

#include "core/config/AppConfig.h"

namespace fincept::autotrade {

QString api_base_url() {
    QString url = qEnvironmentVariable("FINCEPT_AUTOTRADE_URL");
    if (url.isEmpty())
        url = AppConfig::instance().get("autotrade/api_url").toString();
    if (url.isEmpty())
        url = QStringLiteral("http://localhost:8000");
    while (url.endsWith('/'))
        url.chop(1);
    return url;
}

QString api_url(const QString& path) {
    return api_base_url() + path;
}

} // namespace fincept::autotrade
