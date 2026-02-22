# Autotrade Broker Integration for Fincept Terminal

## Overview

The Autotrade integration provides read-only access to Autotrade portfolio data through the Fincept Terminal. This integration allows users to view their Autotrade portfolio positions, holdings, and performance within the Fincept Terminal's Portfolio tab.

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Autotrade Integration Flow                       │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  1. Tauri Commands (Rust Backend)                                   │
│     - autotrade_get_positions()                                     │
│     - autotrade_get_account_summary()                               │
│     - autotrade_get_orders()                                        │
│     - autotrade_get_performance()                                   │
│     - Fetches data from http://localhost:8001                      │
│                                                                     │
│  2. TypeScript Adapter (AutotradeAdapter)                          │
│     - Implements IStockBrokerAdapter interface                      │
│     - Transforms Autotrade data to unified broker format            │
│     - Handles authentication and session management                 │
│     - Provides methods: getPositions(), getHoldings(), getFunds()   │
│                                                                     │
│  3. Portfolio Service (autotradeService.ts)                        │
│     - Bridges Autotrade adapter to Fincept portfolio system         │
│     - Calculates portfolio metrics and holdings                     │
│     - Exposes getPortfolioSummary() for UI consumption              │
│                                                                     │
│  4. React UI (PortfolioTab)                                        │
│     - Displays 'AUTO' sub-tab for Autotrade                         │
│     - Shows positions, P&L, performance metrics                    │
│     - Uses unified broker context for consistency                   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## Files

### Rust Backend (src-tauri/src/commands/brokers/autotrade.rs)
- `autotrade_get_positions` - Fetch positions from Autotrade API
- `autotrade_get_account_summary` - Get portfolio summary
- `autotrade_get_orders` - Get order history
- `autotrade_get_performance` - Get performance data

### TypeScript Types (src/brokers/autotrade/types.ts)
- `AutotradePosition` - Extended from Position with autotrade fields
- `AutotradeHolding` - Extended from Holding for portfolio display
- `AutotradePortfolioSummary` - Matches /api/v1/portfolio response
- `AutotradePortfolioHolding` - Transformed holding for Fincept Portfolio
- `AutotradePerformance` - Performance metrics

### TypeScript Adapter (src/brokers/autotrade/AutotradeAdapter.ts)
- Implements `IStockBrokerAdapter` interface
- Methods: `getPositions()`, `getHoldings()`, `getFunds()`, `getOrders()`
- Not supported: `placeOrder()`, `modifyOrder()`, `cancelOrder()` (Autotrade uses strategies)

### Portfolio Service (src/services/portfolio/autotradeService.ts)
- `AutotradePortfolioService` class
- Methods: `getPortfolioSummary()`, `getHoldings()`
- Integrates with Fincept's portfolio system

## API Endpoints (Autotrade Integration Service)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v1/positions` | GET | Get positions for account |
| `/api/v1/portfolio` | GET | Get portfolio summary |
| `/api/v1/orders` | GET | Get order history |
| `/api/v1/portfolio/performance` | GET | Get performance data |

## Integration Points

### 1. Broker Registry (`src/brokers/stocks/registry.ts`)
```typescript
autotrade: {
  id: 'autotrade',
  name: 'autotrade',
  displayName: 'Autotrade',
  region: 'us',
  // ... metadata
}
```

### 2. Stock Broker Initialization (`src/brokers/stocks/init.ts`)
```typescript
import { AutotradeAdapter } from '../autotrade';

export function initializeStockBrokers(): void {
  // ... other brokers
  registerBrokerAdapter('autotrade', AutotradeAdapter);
}
```

### 3. Broker Context (`src/contexts/BrokerContext.tsx`)
```typescript
// Autotrade adapter is available through broker context
const { adapter } = useBrokerAdapter();
```

## Usage

### Getting Autotrade Positions
```typescript
import { AutotradeAdapter } from '@/brokers/autotrade';

const adapter = new AutotradeAdapter();
await adapter.authenticate({ userId: 'DU1234567' });

const positions = await adapter.getPositions();
console.log(positions);
```

### Getting Portfolio Summary
```typescript
import { autotradePortfolioService } from '@/services/portfolio/autotradeService';

const summary = await autotradePortfolioService.getPortfolioSummary('DU1234567');
console.log(summary);
```

## Configuration

The Autotrade Integration Service must be running on `http://localhost:8001` before using this integration.

## Limitations

- Manual order placement is not supported (Autotrade uses strategies)
- Historical data (OHLCV) is not available through this adapter
- Market depth data is not available

## Development

To test the integration locally:
1. Start the Autotrade Integration Service on port 8001
2. Run Fincept Terminal: `npm run tauri dev`
3. Navigate to Portfolio tab and select 'AUTO' sub-tab
