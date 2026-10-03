#pragma once
#include <QJsonObject>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace fincept::autotrade {

/// Autotrade > Conditions: today's market/macro conditions (VIX, trend, rates, credit, dollar,
/// inflation, Fed) from the research service's /research/conditions, each placed in its
/// historical third, with the selected strategy's average month and win rate in that third
/// next to QQQ. Descriptive history, not a signal: the research service's filter_test shows
/// whether acting on it would have helped out of sample.
class ConditionsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ConditionsPanel(QWidget* parent = nullptr);

    void apply_styles();
    void retranslate();

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    void load_models();
    void load();
    void render(const QJsonObject& data);

    QWidget* bar_ = nullptr;
    QLabel* model_lbl_ = nullptr;
    QComboBox* model_combo_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QLabel* summary_lbl_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* note_lbl_ = nullptr;
    bool models_loaded_ = false;
};

} // namespace fincept::autotrade
