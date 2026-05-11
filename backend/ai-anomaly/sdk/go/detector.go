package anomaly

import (
	"context"
	"fmt"
	"sync"
	"time"
)

// DetectionMode represents the detection mode
type DetectionMode int

const (
	// ActiveMode - actively call detection API for each event
	ActiveMode DetectionMode = iota
	// PassiveMode - batch and buffer events for periodic detection
	PassiveMode
)

// DetectorConfig holds configuration for the high-level detector
type DetectorConfig struct {
	// ClientConfig is the underlying HTTP client configuration
	ClientConfig ClientConfig

	// Mode is the detection mode (Active or Passive)
	Mode DetectionMode

	// BatchSize is the number of events to batch in Passive mode
	BatchSize int

	// FlushInterval is the time interval to flush batched events in Passive mode
	FlushInterval time.Duration

	// AsyncWorkers is the number of concurrent workers for async detection
	AsyncWorkers int

	// OnAnomaly is the callback for detected anomalies
	OnAnomaly func(detectionType DetectionType, data DetectionData, result *DetectionResult)

	// OnError is the error callback
	OnError func(detectionType DetectionType, data DetectionData, err error)
}

// DefaultDetectorConfig returns the default detector configuration
func DefaultDetectorConfig(baseURL string) DetectorConfig {
	return DetectorConfig{
		ClientConfig: ClientConfig{
			BaseURL: baseURL,
			Timeout: 30 * time.Second,
		},
		Mode:          ActiveMode,
		BatchSize:     100,
		FlushInterval: 5 * time.Second,
		AsyncWorkers:  4,
	}
}

// Detector is the high-level anomaly detector with async, batch, and streaming capabilities
type Detector struct {
	client   *Client
	config   DetectorConfig

	// Passive mode components
	buffer      map[DetectionType][]DetectionData
	bufferMu    sync.RWMutex
	flushTicker *time.Ticker
	stopCh      chan struct{}
	wg          sync.WaitGroup

	// Async mode components
	workCh   chan detectionTask
	resultCh chan detectionResult
}

type detectionTask struct {
	ctx           context.Context
	detectionType DetectionType
	data          DetectionData
	saveAnomaly   bool
}

type detectionResult struct {
	detectionType DetectionType
	data          DetectionData
	result        *DetectionResult
	err           error
}

// NewDetector creates a new high-level detector
func NewDetector(config DetectorConfig) (*Detector, error) {
	client, err := NewClient(config.ClientConfig)
	if err != nil {
		return nil, fmt.Errorf("failed to create client: %w", err)
	}

	d := &Detector{
		client: client,
		config: config,
		buffer: make(map[DetectionType][]DetectionData),
		stopCh: make(chan struct{}),
		workCh: make(chan detectionTask, config.BatchSize*2),
		resultCh: make(chan detectionResult, config.BatchSize*2),
	}

	// Initialize buffers for each detection type
	for _, t := range []DetectionType{DetectionKey, DetectionVehicle, DetectionUser} {
		d.buffer[t] = make([]DetectionData, 0, config.BatchSize)
	}

	// Start background workers
	if config.Mode == PassiveMode {
		d.startPassiveMode()
	}
	d.startAsyncWorkers()

	return d, nil
}

// startPassiveMode starts the passive mode background processing
func (d *Detector) startPassiveMode() {
	d.flushTicker = time.NewTicker(d.config.FlushInterval)

	d.wg.Add(1)
	go func() {
		defer d.wg.Done()
		for {
			select {
			case <-d.flushTicker.C:
				d.flushAll()
			case <-d.stopCh:
				return
			}
		}
	}()
}

// startAsyncWorkers starts the async detection workers
func (d *Detector) startAsyncWorkers() {
	for i := 0; i < d.config.AsyncWorkers; i++ {
		d.wg.Add(1)
		go d.asyncWorker(i)
	}

	// Start result processor
	d.wg.Add(1)
	go d.resultProcessor()
}

// asyncWorker processes detection tasks asynchronously
func (d *Detector) asyncWorker(id int) {
	defer d.wg.Done()

	for task := range d.workCh {
		var result *DetectionResult
		var err error

		switch task.detectionType {
		case DetectionKey:
			result, err = d.client.DetectKey(task.ctx, task.data, task.saveAnomaly)
		case DetectionVehicle:
			result, err = d.client.DetectVehicle(task.ctx, task.data, task.saveAnomaly)
		case DetectionUser:
			result, err = d.client.DetectUser(task.ctx, task.data, task.saveAnomaly)
		}

		d.resultCh <- detectionResult{
			detectionType: task.detectionType,
			data:          task.data,
			result:        result,
			err:           err,
		}
	}
}

// resultProcessor processes detection results and invokes callbacks
func (d *Detector) resultProcessor() {
	defer d.wg.Done()

	for res := range d.resultCh {
		if res.err != nil {
			if d.config.OnError != nil {
				d.config.OnError(res.detectionType, res.data, res.err)
			}
			continue
		}

		if res.result != nil && res.result.IsAnomaly {
			if d.config.OnAnomaly != nil {
				d.config.OnAnomaly(res.detectionType, res.data, res.result)
			}
		}
	}
}

// Detect performs synchronous anomaly detection based on the configured mode
func (d *Detector) Detect(ctx context.Context, detectionType DetectionType, data DetectionData, saveAnomaly bool) (*DetectionResult, error) {
	if d.config.Mode == PassiveMode {
		// In passive mode, add to buffer and return nil
		d.addToBuffer(detectionType, data)
		return nil, nil
	}

	// Active mode - direct API call
	switch detectionType {
	case DetectionKey:
		return d.client.DetectKey(ctx, data, saveAnomaly)
	case DetectionVehicle:
		return d.client.DetectVehicle(ctx, data, saveAnomaly)
	case DetectionUser:
		return d.client.DetectUser(ctx, data, saveAnomaly)
	default:
		return nil, fmt.Errorf("unknown detection type: %s", detectionType)
	}
}

// DetectAsync performs asynchronous anomaly detection
func (d *Detector) DetectAsync(ctx context.Context, detectionType DetectionType, data DetectionData, saveAnomaly bool) {
	task := detectionTask{
		ctx:           ctx,
		detectionType: detectionType,
		data:          data,
		saveAnomaly:   saveAnomaly,
	}

	select {
	case d.workCh <- task:
		// Task queued successfully
	case <-ctx.Done():
		// Context cancelled
		if d.config.OnError != nil {
			d.config.OnError(detectionType, data, ctx.Err())
		}
	}
}

// addToBuffer adds data to the buffer for the specified detection type
func (d *Detector) addToBuffer(detectionType DetectionType, data DetectionData) {
	d.bufferMu.Lock()
	defer d.bufferMu.Unlock()

	d.buffer[detectionType] = append(d.buffer[detectionType], data)

	// Check if buffer is full
	if len(d.buffer[detectionType]) >= d.config.BatchSize {
		go d.flush(detectionType)
	}
}

// flush processes buffered data for a specific detection type
func (d *Detector) flush(detectionType DetectionType) {
	d.bufferMu.Lock()
	if len(d.buffer[detectionType]) == 0 {
		d.bufferMu.Unlock()
		return
	}

	// Copy and clear buffer
	data := make([]DetectionData, len(d.buffer[detectionType]))
	copy(data, d.buffer[detectionType])
	d.buffer[detectionType] = d.buffer[detectionType][:0]
	d.bufferMu.Unlock()

	// Perform batch detection
	ctx := context.Background()
	req := BatchDetectionRequest{
		DetectionType: detectionType,
		Data:          data,
		Threshold:     0.5,
	}

	result, err := d.client.BatchDetect(ctx, req)
	if err != nil {
		if d.config.OnError != nil {
			for _, d := range data {
				d.config.OnError(detectionType, d, err)
			}
		}
		return
	}

	// Process results
	if d.config.OnAnomaly != nil {
		for i, r := range result.Results {
			if r.IsAnomaly {
				d.config.OnAnomaly(detectionType, data[i], &r)
			}
		}
	}
}

// flushAll processes all buffered data
func (d *Detector) flushAll() {
	for _, t := range []DetectionType{DetectionKey, DetectionVehicle, DetectionUser} {
		d.flush(t)
	}
}

// BatchDetect performs batch detection on multiple data points
func (d *Detector) BatchDetect(ctx context.Context, detectionType DetectionType, data []DetectionData, threshold float64) (*BatchDetectionResponse, error) {
	req := BatchDetectionRequest{
		DetectionType: detectionType,
		Data:          data,
		Threshold:     threshold,
	}
	return d.client.BatchDetect(ctx, req)
}

// StreamDetect performs stream detection on multiple data points
func (d *Detector) StreamDetect(ctx context.Context, detectionType DetectionType, dataPoints []DetectionData) (*StreamDetectionResponse, error) {
	return d.client.SimulateStreamDetection(ctx, detectionType, dataPoints)
}

// DetectKey is a convenience method for key anomaly detection
func (d *Detector) DetectKey(ctx context.Context, data DetectionData, saveAnomaly bool) (*DetectionResult, error) {
	return d.Detect(ctx, DetectionKey, data, saveAnomaly)
}

// DetectVehicle is a convenience method for vehicle anomaly detection
func (d *Detector) DetectVehicle(ctx context.Context, data DetectionData, saveAnomaly bool) (*DetectionResult, error) {
	return d.Detect(ctx, DetectionVehicle, data, saveAnomaly)
}

// DetectUser is a convenience method for user behavior anomaly detection
func (d *Detector) DetectUser(ctx context.Context, data DetectionData, saveAnomaly bool) (*DetectionResult, error) {
	return d.Detect(ctx, DetectionUser, data, saveAnomaly)
}

// DetectKeyAsync is a convenience method for async key anomaly detection
func (d *Detector) DetectKeyAsync(ctx context.Context, data DetectionData, saveAnomaly bool) {
	d.DetectAsync(ctx, DetectionKey, data, saveAnomaly)
}

// DetectVehicleAsync is a convenience method for async vehicle anomaly detection
func (d *Detector) DetectVehicleAsync(ctx context.Context, data DetectionData, saveAnomaly bool) {
	d.DetectAsync(ctx, DetectionVehicle, data, saveAnomaly)
}

// DetectUserAsync is a convenience method for async user behavior anomaly detection
func (d *Detector) DetectUserAsync(ctx context.Context, data DetectionData, saveAnomaly bool) {
	d.DetectAsync(ctx, DetectionUser, data, saveAnomaly)
}

// HealthCheck checks the health of the anomaly detection service
func (d *Detector) HealthCheck(ctx context.Context) (*HealthCheckResponse, error) {
	return d.client.HealthCheck(ctx)
}

// GetStats retrieves statistics overview
func (d *Detector) GetStats(ctx context.Context) (*StatsOverview, error) {
	return d.client.GetStatsOverview(ctx)
}

// GetClient returns the underlying HTTP client
func (d *Detector) GetClient() *Client {
	return d.client
}

// Close gracefully shuts down the detector
func (d *Detector) Close() error {
	// Stop background goroutines
	close(d.stopCh)

	if d.flushTicker != nil {
		d.flushTicker.Stop()
	}

	// Flush remaining data
	if d.config.Mode == PassiveMode {
		d.flushAll()
	}

	// Close work channel
	close(d.workCh)

	// Wait for all workers to finish
	d.wg.Wait()

	// Close result channel
	close(d.resultCh)

	// Close HTTP client
	return d.client.Close()
}

// =============================================================================
// Builder Pattern for Detector Configuration
// =============================================================================

// DetectorBuilder helps build a detector with fluent API
type DetectorBuilder struct {
	config DetectorConfig
}

// NewDetectorBuilder creates a new detector builder
func NewDetectorBuilder(baseURL string) *DetectorBuilder {
	return &DetectorBuilder{
		config: DefaultDetectorConfig(baseURL),
	}
}

// WithAPIKey sets the API key
func (b *DetectorBuilder) WithAPIKey(apiKey string) *DetectorBuilder {
	b.config.ClientConfig.APIKey = apiKey
	return b
}

// WithTimeout sets the timeout
func (b *DetectorBuilder) WithTimeout(timeout time.Duration) *DetectorBuilder {
	b.config.ClientConfig.Timeout = timeout
	return b
}

// WithMode sets the detection mode
func (b *DetectorBuilder) WithMode(mode DetectionMode) *DetectorBuilder {
	b.config.Mode = mode
	return b
}

// WithBatchSize sets the batch size for passive mode
func (b *DetectorBuilder) WithBatchSize(size int) *DetectorBuilder {
	b.config.BatchSize = size
	return b
}

// WithFlushInterval sets the flush interval for passive mode
func (b *DetectorBuilder) WithFlushInterval(interval time.Duration) *DetectorBuilder {
	b.config.FlushInterval = interval
	return b
}

// WithAsyncWorkers sets the number of async workers
func (b *DetectorBuilder) WithAsyncWorkers(count int) *DetectorBuilder {
	b.config.AsyncWorkers = count
	return b
}

// WithAnomalyCallback sets the anomaly callback
func (b *DetectorBuilder) WithAnomalyCallback(callback func(DetectionType, DetectionData, *DetectionResult)) *DetectorBuilder {
	b.config.OnAnomaly = callback
	return b
}

// WithErrorCallback sets the error callback
func (b *DetectorBuilder) WithErrorCallback(callback func(DetectionType, DetectionData, error)) *DetectorBuilder {
	b.config.OnError = callback
	return b
}

// WithRetryConfig sets the retry configuration
func (b *DetectorBuilder) WithRetryConfig(retryConfig *RetryConfig) *DetectorBuilder {
	b.config.ClientConfig.RetryConfig = retryConfig
	return b
}

// Build creates the detector
func (b *DetectorBuilder) Build() (*Detector, error) {
	return NewDetector(b.config)
}
