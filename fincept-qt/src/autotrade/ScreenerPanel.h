#pragma once
#include <QWidget>

class QLabel;
class QPushButton;

namespace fincept::autotrade {

/// Autotrade > Screener: the research screener view (presets, stages, SEC financials, InvestingPro health),
/// embedded with Qt WebEngine when the build has it, otherwise a button that opens it in the browser.
/// Address: FINCEPT_SCREENER_URL, setting autotrade/screener_url, or http://autotrade.lan/research/ui.
/// Loaded the first time the tab is shown.
class ScreenerPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ScreenerPanel(QWidget* parent = nullptr);

    void apply_styles();
    void retranslate();
    static QString screener_url();

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    QWidget* bar_ = nullptr;
    QLabel* title_lbl_ = nullptr;
    QPushButton* reload_btn_ = nullptr;
    QPushButton* open_btn_ = nullptr;
    QWidget* view_ = nullptr;   // QWebEngineView when available
    QLabel* fallback_lbl_ = nullptr;
    bool loaded_ = false;
};

} // namespace fincept::autotrade
