#include "autotrade/OptionsPanel.h"

#include "core/config/AppConfig.h"
#include "ui/theme/Theme.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#ifdef HAS_QT_WEBENGINE
#    include <QWebEngineView>
#endif

namespace fincept::autotrade {

QString OptionsPanel::workstation_url() {
    QString url = qEnvironmentVariable("FINCEPT_OPTIONS_URL");
    if (url.isEmpty())
        url = AppConfig::instance().get("autotrade/options_url").toString();
    return url.isEmpty() ? QStringLiteral("http://options.lan") : url;
}

OptionsPanel::OptionsPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    bar_ = new QWidget(this);
    auto* hb = new QHBoxLayout(bar_);
    hb->setContentsMargins(12, 6, 12, 6);
    title_lbl_ = new QLabel;
    title_lbl_->setTextFormat(Qt::RichText);
    hb->addWidget(title_lbl_, 1);
    reload_btn_ = new QPushButton;
    reload_btn_->setCursor(Qt::PointingHandCursor);
    hb->addWidget(reload_btn_);
    open_btn_ = new QPushButton;
    open_btn_->setCursor(Qt::PointingHandCursor);
    connect(open_btn_, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(workstation_url())); });
    hb->addWidget(open_btn_);
    root->addWidget(bar_);

#ifdef HAS_QT_WEBENGINE
    auto* web = new QWebEngineView(this);
    view_ = web;
    connect(reload_btn_, &QPushButton::clicked, web, &QWebEngineView::reload);
    root->addWidget(web, 1);
#else
    reload_btn_->hide();
    fallback_lbl_ = new QLabel(this);
    fallback_lbl_->setAlignment(Qt::AlignCenter);
    fallback_lbl_->setWordWrap(true);
    root->addWidget(fallback_lbl_, 1);
#endif
    retranslate();
}

void OptionsPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
#ifdef HAS_QT_WEBENGINE
    if (!loaded_) {
        static_cast<QWebEngineView*>(view_)->load(QUrl(workstation_url()));
        loaded_ = true;
    }
#endif
}

void OptionsPanel::apply_styles() {
    if (bar_)
        bar_->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    if (title_lbl_)
        title_lbl_->setStyleSheet(QString("color: %1; font-size: 11px; border: none;").arg(ui::colors::TEXT_SECONDARY()));
    if (fallback_lbl_)
        fallback_lbl_->setStyleSheet(QString("color: %1; font-size: 12px;").arg(ui::colors::TEXT_TERTIARY()));
}

void OptionsPanel::retranslate() {
    title_lbl_->setText(tr("<b>OPTION WORKSTATION</b> · replay of point-in-time option chains · %1")
                            .arg(workstation_url().toHtmlEscaped()));
    reload_btn_->setText(tr("RELOAD"));
    open_btn_->setText(tr("OPEN IN BROWSER"));
    if (fallback_lbl_)
        fallback_lbl_->setText(tr("This build has no embedded web view (Qt WebEngine). Use OPEN IN BROWSER to view the "
                                  "Option Workstation."));
}

} // namespace fincept::autotrade
