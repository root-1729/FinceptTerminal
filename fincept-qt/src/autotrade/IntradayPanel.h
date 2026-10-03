#pragma once
#include <QJsonObject>
#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;
class QTimer;

namespace fincept::autotrade {

/// Autotrade > Intraday: the intraday momentum (noise area) strategy on SPY, from the
/// api-gateway's /intraday/summary. Mode, position and capital; the latest day's decisions
/// (bands, VWAP, side, orders) and fills (price, commission, slippage); and every recorded
/// day's real result next to what the strategy's backtest gives for the same day.
/// Refreshes every 30 seconds while visible.
class IntradayPanel : public QWidget {
    Q_OBJECT
  public:
    explicit IntradayPanel(QWidget* parent = nullptr);

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
    QLabel* totals_lbl_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QLabel* decisions_title_ = nullptr;
    QTableWidget* decisions_ = nullptr;
    QLabel* fills_title_ = nullptr;
    QTableWidget* fills_ = nullptr;
    QLabel* days_title_ = nullptr;
    QTableWidget* days_ = nullptr;
    QLabel* note_lbl_ = nullptr;
    QTimer* timer_ = nullptr;
};

} // namespace fincept::autotrade
