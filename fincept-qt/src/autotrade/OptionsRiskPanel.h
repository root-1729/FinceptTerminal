#pragma once
#include <QJsonObject>
#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;
class QTimer;

namespace fincept::autotrade {

/// Autotrade > Options risk: every option position in every broker account, from the api-gateway's
/// /platform/options-book. Positions priced off the market data hub's latest chain (mark, IV and
/// position Greeks), share hedges in the same underlyings, the book's Greeks and its stress test
/// (spot and vol shocks, full revaluation; the same numbers the pre-trade stress check uses).
/// Read-only. Refreshes every 60 seconds while visible.
class OptionsRiskPanel : public QWidget {
    Q_OBJECT
  public:
    explicit OptionsRiskPanel(QWidget* parent = nullptr);

    void apply_styles();
    void retranslate();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void load();
    void render(const QJsonObject& data);

    QWidget* bar_ = nullptr;
    QLabel* summary_lbl_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QLabel* positions_title_ = nullptr;
    QTableWidget* positions_ = nullptr;
    QLabel* stress_title_ = nullptr;
    QTableWidget* stress_ = nullptr;
    QLabel* note_lbl_ = nullptr;
    QTimer* timer_ = nullptr;
};

} // namespace fincept::autotrade
