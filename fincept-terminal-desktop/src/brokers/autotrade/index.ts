/**
 * Autotrade Broker Module
 *
 * Exports all Autotrade-related types and implementations
 */

export { AutotradeAdapter, autotradeBrokerMetadata } from './AutotradeAdapter';
export type {
  AutotradePosition,
  AutotradeHolding,
  AutotradePortfolioSummary,
  AutotradePortfolioHolding,
  AutotradePerformance,
  AutotradePerformanceSeries,
  AutotradeOrder,
  AutotradeOrderResponse,
  AutotradeQuote,
  AutotradeInstrument,
  AutotradeApiResponse,
  AutotradePositionsResponse,
  AutotradePortfolioResponse,
  AutotradeAccount,
} from './types';

// Re-export utility functions
export { transformAutotradePositionToPosition, transformAutotradePortfolioToFincept } from './types';

// Export sample data for testing
export { sampleAutotradePosition, sampleAutotradePortfolio } from './types';
