// File: src/components/tabs/autotrade/AutotradeScreenerPanel.tsx
// Screener integration panel using Autotrade's database-backed screener service

import React, { useState, useEffect } from 'react';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Label } from '@/components/ui/label';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select';
import { Filter, Play, RefreshCw, TrendingUp, TrendingDown } from 'lucide-react';

const AUTOTRADE_API = 'http://localhost:8000';

interface ScreenerConfig {
  filename: string;
  name: string;
  description: string;
  instrument: string;
  scan_type: string;
}

interface ScreenerResult {
  symbol: string;
  company_name: string;
  exchange: string;
  price: number;
  change_amount: number;
  change_percent: number;
  volume: number | null;
  market_cap: number | null;
  pe_ratio: number | null;
}

interface ScreenerSession {
  session_id: string;
  config_name: string;
  screen_name: string;
  results_count: number;
  status: string;
  created_at: string;
  completed_at: string | null;
}

export default function AutotradeScreenerPanel() {
  const [configs, setConfigs] = useState<ScreenerConfig[]>([]);
  const [selectedConfig, setSelectedConfig] = useState<string>('');
  const [results, setResults] = useState<ScreenerResult[]>([]);
  const [session, setSession] = useState<ScreenerSession | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const [isLoadingConfigs, setIsLoadingConfigs] = useState(true);

  // Load available screener configs on mount
  useEffect(() => {
    console.log('[AutotradeScreenerPanel] Component mounted, loading configs...');
    loadConfigs();
  }, []);

  // Load results when config is selected
  useEffect(() => {
    if (selectedConfig) {
      console.log('[AutotradeScreenerPanel] Config selected:', selectedConfig);
      loadLatestResults();
    }
  }, [selectedConfig]);

  const loadConfigs = async () => {
    console.log('[AutotradeScreenerPanel] loadConfigs() called');
    setIsLoadingConfigs(true);
    try {
      const url = `${AUTOTRADE_API}/screener/configs`;
      console.log('[AutotradeScreenerPanel] Fetching configs from:', url);
      const response = await fetch(url);
      console.log('[AutotradeScreenerPanel] Configs response status:', response.status);
      
      if (!response.ok) {
        throw new Error(`Failed to load configs: ${response.status}`);
      }
      const data = await response.json();
      console.log('[AutotradeScreenerPanel] Configs loaded:', data.configs?.length, 'configs');
      setConfigs(data.configs || []);
      
      // Auto-select first config if available
      if (data.configs && data.configs.length > 0) {
        console.log('[AutotradeScreenerPanel] Auto-selecting first config:', data.configs[0].name);
        setSelectedConfig(data.configs[0].name);
      }
    } catch (error) {
      console.error('[AutotradeScreenerPanel] Failed to load screener configs:', error);
    } finally {
      setIsLoadingConfigs(false);
      console.log('[AutotradeScreenerPanel] loadConfigs() completed');
    }
  };

  const loadLatestResults = async () => {
    if (!selectedConfig) return;
    
    setIsLoading(true);
    try {
      const response = await fetch(
        `${AUTOTRADE_API}/screener/latest?config_name=${encodeURIComponent(selectedConfig)}&limit=50`
      );
      
      if (!response.ok) {
        throw new Error(`Failed to load results: ${response.status}`);
      }
      
      const data = await response.json();
      setSession(data.session);
      setResults(data.results || []);
    } catch (error) {
      console.error('Failed to load screener results:', error);
      setResults([]);
      setSession(null);
    } finally {
      setIsLoading(false);
    }
  };

  const triggerNewScreening = async () => {
    if (!selectedConfig) return;
    
    setIsLoading(true);
    try {
      // Find the config filename
      const config = configs.find(c => c.name === selectedConfig);
      if (!config) {
        throw new Error('Config not found');
      }

      const response = await fetch(`${AUTOTRADE_API}/screener/run`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          config_name: config.filename,
          fetch_quotes: true
        })
      });

      if (!response.ok) {
        throw new Error(`Failed to trigger screening: ${response.status}`);
      }

      // Wait a bit for screening to complete, then reload
      setTimeout(() => {
        loadLatestResults();
      }, 5000);
    } catch (error) {
      console.error('Failed to trigger screening:', error);
      alert(`Failed to trigger screening: ${error instanceof Error ? error.message : String(error)}`);
    } finally {
      setIsLoading(false);
    }
  };

  const formatNumber = (num: number | null | undefined): string => {
    if (num === null || num === undefined) return 'N/A';
    return num.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 });
  };

  const formatPercent = (num: number | null | undefined): string => {
    if (num === null || num === undefined) return 'N/A';
    const sign = num >= 0 ? '+' : '';
    return `${sign}${num.toFixed(2)}%`;
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {/* Screener Selection */}
      <div style={{
        backgroundColor: '#0F0F0F',
        border: '1px solid #2A2A2A',
        borderRadius: '8px',
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
          <Filter className="h-5 w-5" />
          STOCK SCREENER
        </h3>
        <div style={{ display: 'flex', gap: '16px', alignItems: 'flex-end' }}>
          <div style={{ flex: 1 }}>
            <label style={{ fontSize: '12px', color: '#787878', display: 'block', marginBottom: '8px' }}>
              Select Screener
            </label>
            <Select value={selectedConfig} onValueChange={setSelectedConfig} disabled={isLoadingConfigs}>
              <SelectTrigger style={{
                backgroundColor: '#1A1A1A',
                border: '1px solid #2A2A2A',
                color: '#FFFFFF',
                height: '40px'
              }}>
                <SelectValue placeholder={isLoadingConfigs ? "Loading configs..." : "Select a screener"} />
              </SelectTrigger>
              <SelectContent style={{
                backgroundColor: '#1A1A1A',
                border: '1px solid #2A2A2A',
                color: '#FFFFFF'
              }}>
                {configs.map((config) => (
                  <SelectItem key={config.filename} value={config.name} style={{
                    color: '#FFFFFF',
                    cursor: 'pointer'
                  }}>
                    {config.name}
                  </SelectItem>
                ))}
              </SelectContent>
            </Select>
            {selectedConfig && configs.find(c => c.name === selectedConfig) && (
              <p style={{ fontSize: '11px', color: '#787878', marginTop: '4px' }}>
                {configs.find(c => c.name === selectedConfig)?.description}
              </p>
            )}
          </div>
          
          <div style={{ display: 'flex', gap: '8px' }}>
            <Button 
              onClick={loadLatestResults} 
              variant="outline" 
              disabled={isLoading || !selectedConfig}
              style={{
                backgroundColor: '#1A1A1A',
                border: '1px solid #2A2A2A',
                color: '#FFFFFF',
                height: '40px',
                padding: '0 16px'
              }}
            >
              <RefreshCw className={`h-4 w-4 mr-2 ${isLoading ? 'animate-spin' : ''}`} />
              Refresh
            </Button>
            <Button 
              onClick={triggerNewScreening} 
              disabled={isLoading || !selectedConfig}
              style={{
                backgroundColor: '#FF8800',
                border: 'none',
                color: '#000000',
                height: '40px',
                padding: '0 16px',
                fontWeight: 600
              }}
            >
              <Play className="h-4 w-4 mr-2" />
              Run New Scan
            </Button>
          </div>
        </div>

        {session && (
          <div style={{
            marginTop: '16px',
            padding: '12px',
            backgroundColor: '#1A1A1A',
            border: '1px solid #2A2A2A',
            borderRadius: '4px'
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              fontSize: '12px'
            }}>
              <div>
                <span style={{ color: '#787878' }}>Last scan: </span>
                <span style={{ color: '#FFFFFF', fontWeight: 500 }}>
                  {new Date(session.completed_at || session.created_at).toLocaleString()}
                </span>
              </div>
              <div>
                <span style={{ color: '#787878' }}>Results: </span>
                <span style={{ color: '#00D66F', fontWeight: 700, fontSize: '14px' }}>
                  {session.results_count}
                </span>
              </div>
            </div>
          </div>
        )}
      </div>

      {/* Loading State */}
      {isLoading && (
        <div style={{
          backgroundColor: '#0F0F0F',
          border: '1px solid #2A2A2A',
          borderRadius: '8px',
          padding: '48px',
          textAlign: 'center'
        }}>
          <RefreshCw className="h-8 w-8 animate-spin" style={{ 
            color: '#FF8800', 
            margin: '0 auto 16px' 
          }} />
          <p style={{ color: '#787878', fontSize: '14px' }}>Loading screener results...</p>
        </div>
      )}
      
      {/* Empty State */}
      {!isLoading && !selectedConfig && (
        <div style={{
          backgroundColor: '#0F0F0F',
          border: '1px solid #2A2A2A',
          borderRadius: '8px',
          padding: '48px',
          textAlign: 'center'
        }}>
          <Filter className="h-12 w-12" style={{ 
            color: '#787878', 
            margin: '0 auto 16px' 
          }} />
          <p style={{ color: '#787878', fontSize: '14px' }}>Select a screener to view results</p>
        </div>
      )}

      {/* No Results State */}
      {!isLoading && selectedConfig && results.length === 0 && (
        <div style={{
          backgroundColor: '#0F0F0F',
          border: '1px solid #2A2A2A',
          borderRadius: '8px',
          padding: '48px',
          textAlign: 'center'
        }}>
          <p style={{ color: '#787878', fontSize: '14px' }}>
            No results available. Click "Run New Scan" to generate fresh results.
          </p>
        </div>
      )}
      
      {/* Results Table */}
      {!isLoading && results.length > 0 && (
        <div style={{
          backgroundColor: '#0F0F0F',
          border: '1px solid #2A2A2A',
          borderRadius: '8px',
          overflow: 'hidden'
        }}>
          <div style={{
            padding: '16px',
            borderBottom: '1px solid #2A2A2A'
          }}>
            <h3 style={{
              fontSize: '14px',
              fontWeight: 600,
              color: '#FF8800'
            }}>
              RESULTS ({results.length} stocks)
            </h3>
          </div>
          <div style={{ overflowX: 'auto' }}>
            <table style={{ width: '100%', borderCollapse: 'collapse' }}>
              <thead>
                <tr style={{ borderBottom: '1px solid #2A2A2A' }}>
                  <th style={{ 
                    textAlign: 'left', 
                    padding: '12px 16px', 
                    color: '#787878', 
                    fontSize: '11px',
                    fontWeight: 600,
                    textTransform: 'uppercase'
                  }}>Symbol</th>
                  <th style={{ 
                    textAlign: 'left', 
                    padding: '12px 16px', 
                    color: '#787878', 
                    fontSize: '11px',
                    fontWeight: 600,
                    textTransform: 'uppercase'
                  }}>Company</th>
                  <th style={{ 
                    textAlign: 'left', 
                    padding: '12px 16px', 
                    color: '#787878', 
                    fontSize: '11px',
                    fontWeight: 600,
                    textTransform: 'uppercase'
                  }}>Exchange</th>
                  <th style={{ 
                    textAlign: 'right', 
                    padding: '12px 16px', 
                    color: '#787878', 
                    fontSize: '11px',
                    fontWeight: 600,
                    textTransform: 'uppercase'
                  }}>Price</th>
                  <th style={{ 
                    textAlign: 'right', 
                    padding: '12px 16px', 
                    color: '#787878', 
                    fontSize: '11px',
                    fontWeight: 600,
                    textTransform: 'uppercase'
                  }}>Change</th>
                  <th style={{ 
                    textAlign: 'right', 
                    padding: '12px 16px', 
                    color: '#787878', 
                    fontSize: '11px',
                    fontWeight: 600,
                    textTransform: 'uppercase'
                  }}>Change %</th>
                  <th style={{ 
                    textAlign: 'right', 
                    padding: '12px 16px', 
                    color: '#787878', 
                    fontSize: '11px',
                    fontWeight: 600,
                    textTransform: 'uppercase'
                  }}>Volume</th>
                </tr>
              </thead>
              <tbody>
                {results.map((stock, idx) => (
                  <tr key={`${stock.symbol}-${idx}`} style={{ 
                    borderBottom: '1px solid #1A1A1A',
                    cursor: 'pointer',
                    transition: 'background-color 0.2s'
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#1A1A1A'}
                  onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
                  >
                    <td style={{ padding: '12px 16px' }}>
                      <span style={{ 
                        color: '#FF8800', 
                        fontWeight: 700,
                        fontSize: '13px'
                      }}>{stock.symbol}</span>
                    </td>
                    <td style={{ padding: '12px 16px', fontSize: '12px', color: '#FFFFFF' }}>
                      {stock.company_name}
                    </td>
                    <td style={{ padding: '12px 16px', fontSize: '10px', color: '#787878' }}>
                      {stock.exchange}
                    </td>
                    <td style={{ 
                      padding: '12px 16px', 
                      textAlign: 'right',
                      fontFamily: 'monospace',
                      fontSize: '13px',
                      color: '#FFFFFF',
                      fontWeight: 600
                    }}>
                      ${formatNumber(stock.price)}
                    </td>
                    <td style={{ 
                      padding: '12px 16px', 
                      textAlign: 'right',
                      fontFamily: 'monospace',
                      fontSize: '13px',
                      color: stock.change_amount >= 0 ? '#00D66F' : '#FF3B3B',
                      fontWeight: 600
                    }}>
                      {formatNumber(stock.change_amount)}
                    </td>
                    <td style={{ padding: '12px 16px', textAlign: 'right' }}>
                      <div style={{ 
                        display: 'flex', 
                        alignItems: 'center', 
                        justifyContent: 'flex-end', 
                        gap: '4px' 
                      }}>
                        {stock.change_percent >= 0 ? (
                          <TrendingUp style={{ 
                            width: '16px', 
                            height: '16px', 
                            color: '#00D66F' 
                          }} />
                        ) : (
                          <TrendingDown style={{ 
                            width: '16px', 
                            height: '16px', 
                            color: '#FF3B3B' 
                          }} />
                        )}
                        <span style={{ 
                          fontWeight: 700,
                          fontSize: '13px',
                          color: stock.change_percent >= 0 ? '#00D66F' : '#FF3B3B'
                        }}>
                          {formatPercent(stock.change_percent)}
                        </span>
                      </div>
                    </td>
                    <td style={{ 
                      padding: '12px 16px', 
                      textAlign: 'right',
                      fontSize: '12px',
                      color: '#787878'
                    }}>
                      {stock.volume ? stock.volume.toLocaleString() : 'N/A'}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </div>
  );
}
