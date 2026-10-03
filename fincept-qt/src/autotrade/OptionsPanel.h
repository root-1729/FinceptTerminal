#pragma once
#include <QWidget>

class QLabel;
class QPushButton;

namespace fincept::autotrade {

/// Autotrade > Options: the OptionWorkstation replay UI (plan 4.4), embedded with Qt WebEngine when the
/// build has it, otherwise a button that opens it in the browser. Address: FINCEPT_OPTIONS_URL, setting
/// autotrade/options_url, or http://options.lan. Loaded the first time the tab is shown.
class OptionsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit OptionsPanel(QWidget* parent = nullptr);

    void apply_styles();
    void retranslate();
    static QString workstation_url();

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
