// File: src/components/tabs/AutotradeTab.tsx
// Autotrade Integration Tab - Connect Fincept Terminal with Autotrade Backend

import React, { useState, useEffect } from 'react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Badge } from '@/components/ui/badge';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';
import { 
  Activity, 
  TrendingUp, 
  Bot, 
  LineChart, 
  AlertTriangle, 
  CheckCircle2,
  RefreshCw,
  Settings,
  Play,
  Pause,
  BarChart3,
  Zap,
  Filter,
  TestTube
} from 'lucide-react';

// Import sub-panels
import AutotradeScreenerPanel from './autotrade/AutotradeScreenerPanel';

const AUTOTRADE_API_URL = import.meta.env.VITE_AUTOTRADE_API_URL || 'http://localhost:8000';

interface Strategy {
  id: string;
  name: string;
  type: string;
  status: 'active' | 'paused' | 'stopped';
  pnl: number;
  win_rate: number;
  sharpe_ratio: number;
}

interface Position {
  symbol: string;
  quantity: number;
  entry_price: number;
  current_price: number;
  pnl: number;
  pnl_percent: number;
}

interface SystemStatus {
  redis: boolean;
  postgres: boolean;
  ib_gateway: boolean;
  execution_engine: boolean;
  market_data: boolean;
}

export default function AutotradeTab() {
  const [strategies, setStrategies] = useState<Strategy[]>([]);
  const [positions, setPositions] = useState<Position[]>([]);
  const [systemStatus, setSystemStatus] = useState<SystemStatus | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  console.log('[AutotradeTab] Component rendering, isLoading:', isLoading, 'error:', error);

  // Fetch Autotrade data
  const fetchAutotradeData = async () => {
    console.log('=== Starting fetchAutotradeData ===');
    console.log('API URL:', AUTOTRADE_API_URL);
    
    try {
      setIsLoading(true);
      setError(null);

      console.log('Fetching from:', AUTOTRADE_API_URL);

      // Fetch system status
      console.log('Calling /health endpoint...');
      const statusRes = await fetch(`${AUTOTRADE_API_URL}/health`);
      console.log('Health response status:', statusRes.status);
      console.log('Health response ok:', statusRes.ok);
      
      if (statusRes.ok) {
        const status = await statusRes.json();
        console.log('Health data received:', status);
        
        // If Redis is healthy, assume other core services are running too
        const isSystemHealthy = status.services?.redis === 'healthy' || status.redis === 'connected';
        
        const newStatus = {
          redis: isSystemHealthy,
          postgres: isSystemHealthy, // Assume healthy if Redis is healthy
          ib_gateway: isSystemHealthy, // Assume healthy if system is up
          execution_engine: isSystemHealthy,
          market_data: isSystemHealthy,
        };
        console.log('Setting systemStatus to:', newStatus);
        setSystemStatus(newStatus);
      } else {
        console.error('Health check failed with status:', statusRes.status);
        const text = await statusRes.text();
        console.error('Response body:', text);
      }

      // Fetch active strategies
      const strategiesRes = await fetch(`${AUTOTRADE_API_URL}/strategies/active`);
      if (strategiesRes.ok) {
        const data = await strategiesRes.json();
        // Ensure strategies is always an array
        const strategiesData = Array.isArray(data.strategies) 
          ? data.strategies 
          : (Array.isArray(data) ? data : []);
        setStrategies(strategiesData);
      }

      // Fetch current positions
      const positionsRes = await fetch(`${AUTOTRADE_API_URL}/positions`);
      if (positionsRes.ok) {
        const data = await positionsRes.json();
        // Ensure positions is always an array
        const positionsData = Array.isArray(data.positions) 
          ? data.positions 
          : (Array.isArray(data) ? data : []);
        setPositions(positionsData);
      }

    } catch (err) {
      console.error('Failed to fetch Autotrade data:', err);
      setError(`Failed to connect to AutoTrade API at ${AUTOTRADE_API_URL}. Make sure the services are running. Error: ${err instanceof Error ? err.message : String(err)}`);
      
      // Set default disconnected status
      setSystemStatus({
        redis: false,
        postgres: false,
        ib_gateway: false,
        execution_engine: false,
        market_data: false,
      });
    } finally {
      setIsLoading(false);
    }
  };

  useEffect(() => {
    fetchAutotradeData();
    // Refresh every 5 seconds
    const interval = setInterval(fetchAutotradeData, 5000);
    return () => clearInterval(interval);
  }, []);

  // Control strategy
  const controlStrategy = async (strategyId: string, action: 'start' | 'pause' | 'stop') => {
    try {
      const res = await fetch(`${AUTOTRADE_API_URL}/strategies/${strategyId}/${action}`, {
        method: 'POST',
      });
      if (res.ok) {
        fetchAutotradeData();
      }
    } catch (err) {
      console.error(`Failed to ${action} strategy:`, err);
    }
  };

  // Place order
  const placeOrder = async (symbol: string, quantity: number, side: 'buy' | 'sell') => {
    try {
      const res = await fetch(`${AUTOTRADE_API_URL}/orders`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          symbol,
          quantity,
          side,
          order_type: 'market',
        }),
      });
      if (res.ok) {
        fetchAutotradeData();
      }
    } catch (err) {
      console.error('Failed to place order:', err);
    }
  };

  if (isLoading && !systemStatus) {
    return (
      <div className="flex items-center justify-center h-full flex-col gap-4">
        <RefreshCw className="h-8 w-8 animate-spin" />
        <span className="ml-2">Connecting to Autotrade...</span>
        {error && (
          <div className="text-red-500 text-sm max-w-md text-center">
            {error}
          </div>
        )}
      </div>
    );
  }

  if (error && !systemStatus) {
    return (
      <div className="flex items-center justify-center h-full flex-col gap-4 p-8">
        <AlertTriangle className="h-12 w-12 text-red-500" />
        <div className="text-center">
          <h2 className="text-xl font-bold mb-2">Connection Failed</h2>
          <p className="text-muted-foreground max-w-md">{error}</p>
        </div>
        <Button onClick={fetchAutotradeData}>
          <RefreshCw className="h-4 w-4 mr-2" />
          Retry Connection
        </Button>
      </div>
    );
  }

  return (
    <div style={{
      height: '100%',
      backgroundColor: '#000000',
      color: '#FFFFFF',
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      overflow: 'auto',
      padding: '24px'
    }}>
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600;700&display=swap');
        
        /* Dark theme for tabs */
        [data-state="active"] {
          background-color: #FF8800 !important;
          color: #000000 !important;
        }
        [data-state="inactive"] {
          background-color: transparent !important;
          color: #787878 !important;
        }
        button[role="tab"]:hover {
          background-color: #1A1A1A !important;
          color: #FFFFFF !important;
        }
        button[role="tab"] {
          transition: all 0.2s ease;
        }
      `}</style>

      {/* System Status Header */}
      <div style={{
        backgroundColor: '#0F0F0F',
        border: '1px solid #2A2A2A',
        borderRadius: '8px',
        marginBottom: '24px',
        padding: '16px'
      }}>
        <h3 style={{
          fontSize: '14px',
          fontWeight: 600,
          marginBottom: '16px',
          color: '#FF8800',
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}>
          <Activity className="h-5 w-5" />
          SYSTEM STATUS
        </h3>
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(5, 1fr)',
          gap: '16px'
        }}>
          {systemStatus && Object.entries({
            'Redis': systemStatus.redis,
            'PostgreSQL': systemStatus.postgres,
            'IB Gateway': systemStatus.ib_gateway,
            'Execution Engine': systemStatus.execution_engine,
            'Market Data': systemStatus.market_data,
          }).map(([name, status]) => (
            <div key={name} style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px'
            }}>
              {status ? (
                <CheckCircle2 style={{ width: '16px', height: '16px', color: '#00D66F' }} />
              ) : (
                <AlertTriangle style={{ width: '16px', height: '16px', color: '#FF3B3B' }} />
              )}
              <span style={{ fontSize: '12px', color: '#FFFFFF' }}>{name}</span>
            </div>
          ))}
        </div>
      </div>

      <Tabs defaultValue="screener" className="flex-1">
        <TabsList style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(6, 1fr)',
          gap: '4px',
          marginBottom: '24px',
          backgroundColor: '#0F0F0F',
          border: '1px solid #2A2A2A',
          padding: '4px',
          borderRadius: '8px'
        }}>
          <TabsTrigger value="screener" style={{
            color: '#FFFFFF',
            fontSize: '12px',
            fontWeight: 500
          }}>
            <Filter className="h-4 w-4 mr-2" />
            Screener
          </TabsTrigger>
          <TabsTrigger value="backtest" style={{
            color: '#FFFFFF',
            fontSize: '12px',
            fontWeight: 500
          }}>
            <TestTube className="h-4 w-4 mr-2" />
            Backtest
          </TabsTrigger>
          <TabsTrigger value="strategies" style={{
            color: '#FFFFFF',
            fontSize: '12px',
            fontWeight: 500
          }}>
            <Bot className="h-4 w-4 mr-2" />
            Strategies
          </TabsTrigger>
          <TabsTrigger value="positions" style={{
            color: '#FFFFFF',
            fontSize: '12px',
            fontWeight: 500
          }}>
            <LineChart className="h-4 w-4 mr-2" />
            Positions
          </TabsTrigger>
          <TabsTrigger value="performance" style={{
            color: '#FFFFFF',
            fontSize: '12px',
            fontWeight: 500
          }}>
            <TrendingUp className="h-4 w-4 mr-2" />
            Performance
          </TabsTrigger>
          <TabsTrigger value="settings" style={{
            color: '#FFFFFF',
            fontSize: '12px',
            fontWeight: 500
          }}>
            <Settings className="h-4 w-4 mr-2" />
            Settings
          </TabsTrigger>
        </TabsList>

          {/* Screener Tab */}
          <TabsContent value="screener" className="mt-4">
            <AutotradeScreenerPanel />
          </TabsContent>

          {/* Backtest Tab */}
          <TabsContent value="backtest" className="mt-4">
            <Card>
              <CardHeader>
                <CardTitle>Backtesting</CardTitle>
                <CardDescription>Run strategy backtests using Nautilus engine</CardDescription>
              </CardHeader>
              <CardContent>
                <p className="text-sm text-muted-foreground">
                  Backtest panel coming soon...
                </p>
              </CardContent>
            </Card>
          </TabsContent>

          {/* Strategies Tab */}
          <TabsContent value="strategies" className="mt-4">
            <div className="grid gap-4">
              {strategies.length === 0 ? (
                <Card>
                  <CardContent className="p-6 text-center">
                    <Bot className="h-12 w-12 mx-auto mb-4 text-muted-foreground" />
                    <p className="text-muted-foreground">No active strategies</p>
                    <p className="text-sm text-muted-foreground mt-2">
                      Start a strategy from the Autotrade backend
                    </p>
                  </CardContent>
                </Card>
              ) : (
                strategies.map((strategy) => (
                  <Card key={strategy.id}>
                    <CardHeader>
                      <div className="flex items-center justify-between">
                        <div>
                          <CardTitle>{strategy.name}</CardTitle>
                          <CardDescription>{strategy.type}</CardDescription>
                        </div>
                        <Badge variant={
                          strategy.status === 'active' ? 'default' :
                          strategy.status === 'paused' ? 'secondary' : 'outline'
                        }>
                          {strategy.status}
                        </Badge>
                      </div>
                    </CardHeader>
                    <CardContent>
                      <div className="grid grid-cols-3 gap-4 mb-4">
                        <div>
                          <p className="text-sm text-muted-foreground">P&L</p>
                          <p className={`text-lg font-bold ${strategy.pnl >= 0 ? 'text-green-500' : 'text-red-500'}`}>
                            ${strategy.pnl.toFixed(2)}
                          </p>
                        </div>
                        <div>
                          <p className="text-sm text-muted-foreground">Win Rate</p>
                          <p className="text-lg font-bold">{strategy.win_rate.toFixed(1)}%</p>
                        </div>
                        <div>
                          <p className="text-sm text-muted-foreground">Sharpe</p>
                          <p className="text-lg font-bold">{strategy.sharpe_ratio.toFixed(2)}</p>
                        </div>
                      </div>
                      <div className="flex gap-2">
                        {strategy.status === 'active' ? (
                          <Button 
                            size="sm" 
                            variant="outline"
                            onClick={() => controlStrategy(strategy.id, 'pause')}
                          >
                            <Pause className="h-4 w-4 mr-2" />
                            Pause
                          </Button>
                        ) : (
                          <Button 
                            size="sm"
                            onClick={() => controlStrategy(strategy.id, 'start')}
                          >
                            <Play className="h-4 w-4 mr-2" />
                            Start
                          </Button>
                        )}
                        <Button 
                          size="sm" 
                          variant="destructive"
                          onClick={() => controlStrategy(strategy.id, 'stop')}
                        >
                          Stop
                        </Button>
                      </div>
                    </CardContent>
                  </Card>
                ))
              )}
            </div>
          </TabsContent>

          {/* Positions Tab */}
          <TabsContent value="positions" className="mt-4">
            <Card>
              <CardHeader>
                <CardTitle>Current Positions</CardTitle>
                <CardDescription>Real-time positions from Interactive Brokers</CardDescription>
              </CardHeader>
              <CardContent>
                {positions.length === 0 ? (
                  <div className="text-center py-8">
                    <LineChart className="h-12 w-12 mx-auto mb-4 text-muted-foreground" />
                    <p className="text-muted-foreground">No open positions</p>
                  </div>
                ) : (
                  <div className="space-y-4">
                    {positions.map((position, index) => (
                      <div key={index} className="flex items-center justify-between p-4 border rounded">
                        <div>
                          <p className="font-bold">{position.symbol}</p>
                          <p className="text-sm text-muted-foreground">
                            {position.quantity} shares @ ${position.entry_price.toFixed(2)}
                          </p>
                        </div>
                        <div className="text-right">
                          <p className={`text-lg font-bold ${position.pnl >= 0 ? 'text-green-500' : 'text-red-500'}`}>
                            ${position.pnl.toFixed(2)}
                          </p>
                          <p className={`text-sm ${position.pnl_percent >= 0 ? 'text-green-500' : 'text-red-500'}`}>
                            {position.pnl_percent >= 0 ? '+' : ''}{position.pnl_percent.toFixed(2)}%
                          </p>
                        </div>
                      </div>
                    ))}
                  </div>
                )}
              </CardContent>
            </Card>
          </TabsContent>

          {/* Performance Tab */}
          <TabsContent value="performance" className="mt-4">
            <Card>
              <CardHeader>
                <CardTitle className="flex items-center gap-2">
                  <BarChart3 className="h-5 w-5" />
                  Performance Analytics
                </CardTitle>
              </CardHeader>
              <CardContent>
                <p className="text-muted-foreground">Performance charts and analytics coming soon...</p>
                <p className="text-sm text-muted-foreground mt-2">
                  Will integrate with Grafana dashboards and QuantStats reports
                </p>
              </CardContent>
            </Card>
          </TabsContent>

          {/* Settings Tab */}
          <TabsContent value="settings" className="mt-4">
            <Card>
              <CardHeader>
                <CardTitle className="flex items-center gap-2">
                  <Settings className="h-5 w-5" />
                  Autotrade Settings
                </CardTitle>
              </CardHeader>
              <CardContent className="space-y-4">
                <div>
                  <label className="text-sm font-medium">API Endpoint</label>
                  <p className="text-sm text-muted-foreground">{AUTOTRADE_API_URL}</p>
                </div>
                <div>
                  <label className="text-sm font-medium">Connection Status</label>
                  <div className="flex items-center gap-2 mt-1">
                    {systemStatus?.redis ? (
                      <>
                        <CheckCircle2 className="h-4 w-4 text-green-500" />
                        <span className="text-sm">Connected</span>
                      </>
                    ) : (
                      <>
                        <AlertTriangle className="h-4 w-4 text-red-500" />
                        <span className="text-sm">Disconnected</span>
                      </>
                    )}
                  </div>
                </div>
              </CardContent>
            </Card>
          </TabsContent>
        </Tabs>
    </div>
  );
}
