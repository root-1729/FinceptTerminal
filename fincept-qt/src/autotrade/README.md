# Autotrade integration (root-1729 fork)

Adds an **AUTOTRADE** screen to the Qt Fincept Terminal for the autotrade stack
(`root-1729/autotrade`, running on k3s). Branch `autotrade-qt`, based on upstream `main`.

- **OVERVIEW:** service health, IB account summary, positions, open orders, stock screener.
- **STRATEGIES:** every TQQQ/SQQQ rotation model with live paper returns and backtest stats;
  select one to see its daily history and the trades it implies on $100k.
- **Backtesting tab provider "Autotrade":** backtest, optimise and walk-forward any rotation
  model (or custom parameters) on the cluster's research service (`/research`), with the
  tab's own charts and tables. Script: `scripts/Analytics/backtesting/autotrade/`.

The screen only reads from the api-gateway (plus running screener scans). It never places
orders: the autotrade execution engine is the only thing that trades.

## Files

| File | Purpose |
|---|---|
| `AutotradeApi.*` | api-gateway base URL |
| `AutotradeScreen.*` | the screen and its OVERVIEW tab |
| `StrategiesPanel.*` | the STRATEGIES tab |
| `ConditionsPanel.*` | the CONDITIONS tab (market/macro conditions vs the strategy's history) |
| `autotrade.cmake` | adds the sources to the `FinceptTerminal` target; lists the upstream hooks |

All code is in this folder. Upstream files carry only small additions, each marked
`autotrade:` (listed in `autotrade.cmake`): `CMakeLists.txt`, `app/WindowFrame_Setup.cpp`,
`app/DockScreenRouter.cpp`, `ui/navigation/ToolBar.cpp`, `ui/navigation/CommandBar.cpp`,
`ui/navigation/FKeyBar.cpp`, `services/backtesting/BacktestingTypes.h`.

## API address

First match wins:

1. `FINCEPT_AUTOTRADE_URL` environment variable
2. setting `autotrade/api_url`; on macOS:
   `defaults write com.fincept.FinceptTerminal autotrade.api_url -string "http://autotrade.lan"`
   (quit the app first)
3. `http://localhost:8000`

`autotrade.lan` is the home-network route to the api-gateway (see the autotrade repo's
`deploy/README.md`); it needs `192.168.0.36 autotrade.lan` in `/etc/hosts`.

Endpoints used: `/health`, `/account/summary`, `/positions`, `/orders`, `/screener/configs`,
`/screener/latest`, `/screener/run`, `/rotation/strategies`, `/rotation/history`.

## Build (macOS)

Homebrew on the build Mac is broken, so the tools live in a venv:

```bash
python3 -m venv ~/.local/share/fincept-build/venv
~/.local/share/fincept-build/venv/bin/pip install 'cmake>=3.27' ninja aqtinstall
~/.local/share/fincept-build/venv/bin/aqt install-qt mac desktop 6.8.3 clang_64 \
    --outputdir ~/Qt --modules qtcharts qtwebsockets qtmultimedia qtspeech

export PATH=~/.local/share/fincept-build/venv/bin:$PATH
cd fincept-qt
cmake --preset macos-release -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/macos \
    -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3
cmake --build --preset macos-release
open build/macos-release/FinceptTerminal.app
```

`FinceptTerminal --smoke-test` opens every screen and reports any that fail to construct
(it refuses to run while the terminal is PIN-locked).

## Merging upstream

```bash
git fetch upstream
git merge upstream/main     # conflicts, if any, are in the autotrade: lines above
```

Never merge upstream into the old React/Tauri fork `main` (pre-`autotrade-qt`): upstream
deleted that app on 2026-03-17, so a merge removes it.
