// Package anomaly provides Go SDK for yuleDKCS AI Anomaly Detection Service
package anomaly

import (
	"encoding/json"
	"time"
)

// ============================================================================
// Enums
// ============================================================================

// AnomalySeverity represents the severity level of an anomaly
type AnomalySeverity string

const (
	SeverityLow      AnomalySeverity = "low"
	SeverityMedium   AnomalySeverity = "medium"
	SeverityHigh     AnomalySeverity = "high"
	SeverityCritical AnomalySeverity = "critical"
)

// AnomalyStatus represents the status of an anomaly
type AnomalyStatus string

const (
	StatusDetected     AnomalyStatus = "detected"
	StatusConfirmed    AnomalyStatus = "confirmed"
	StatusResolved     AnomalyStatus = "resolved"
	StatusFalsePositive AnomalyStatus = "false_positive"
)

// DetectionType represents the type of anomaly detection
type DetectionType string

const (
	DetectionKey     DetectionType = "key"
	DetectionVehicle DetectionType = "vehicle"
	DetectionUser    DetectionType = "user"
)

// KeyAnomalyType represents types of key anomalies
type KeyAnomalyType string

const (
	KeyAnomalyUnauthorizedUse     KeyAnomalyType = "unauthorized_use"
	KeyAnomalyDistance            KeyAnomalyType = "distance_anomaly"
	KeyAnomalyTimePattern         KeyAnomalyType = "time_pattern_anomaly"
	KeyAnomalyFrequency           KeyAnomalyType = "frequency_anomaly"
	KeyAnomalyDuplicateAccess     KeyAnomalyType = "duplicate_access"
	KeyAnomalySuspendedKeyUsed    KeyAnomalyType = "suspended_key_used"
)

// VehicleAnomalyType represents types of vehicle anomalies
type VehicleAnomalyType string

const (
	VehicleAnomalyUnauthorizedStart  VehicleAnomalyType = "unauthorized_start"
	VehicleAnomalyLocation           VehicleAnomalyType = "location_anomaly"
	VehicleAnomalyDoorStatus         VehicleAnomalyType = "door_status_anomaly"
	VehicleAnomalyWindowStatus       VehicleAnomalyType = "window_status_anomaly"
	VehicleAnomalyTowing             VehicleAnomalyType = "towing_detected"
	VehicleAnomalyTampering          VehicleAnomalyType = "tampering_detected"
)

// UserBehaviorType represents types of user behavior anomalies
type UserBehaviorType string

const (
	UserBehaviorAccessPattern     UserBehaviorType = "access_pattern_anomaly"
	UserBehaviorLocationJump      UserBehaviorType = "location_jump"
	UserBehaviorVelocity          UserBehaviorType = "velocity_anomaly"
	UserBehaviorDeviceSwitch      UserBehaviorType = "device_switch_anomaly"
	UserBehaviorBruteForce        UserBehaviorType = "brute_force_attempt"
	UserBehaviorImpossibleTravel  UserBehaviorType = "impossible_travel"
)

// ModelType represents the type of ML model to use
type ModelType string

const (
	ModelIsolationForest ModelType = "isolation_forest"
	ModelOneClassSVM     ModelType = "one_class_svm"
)

// ============================================================================
// Location Models
// ============================================================================

// Location represents a geographic location
type Location struct {
	Lat float64 `json:"lat"`
	Lng float64 `json:"lng"`
}

// DoorStatus represents vehicle door status
type DoorStatus struct {
	FrontLeft  string `json:"front_left,omitempty"`
	FrontRight string `json:"front_right,omitempty"`
	RearLeft   string `json:"rear_left,omitempty"`
	RearRight  string `json:"rear_right,omitempty"`
}

// WindowStatus represents vehicle window status
type WindowStatus struct {
	FrontLeft  string `json:"front_left,omitempty"`
	FrontRight string `json:"front_right,omitempty"`
	RearLeft   string `json:"rear_left,omitempty"`
	RearRight  string `json:"rear_right,omitempty"`
}

// AccelerometerData represents accelerometer sensor data
type AccelerometerData struct {
	X float64 `json:"x"`
	Y float64 `json:"y"`
	Z float64 `json:"z"`
}

// GyroscopeData represents gyroscope sensor data
type GyroscopeData struct {
	X float64 `json:"x"`
	Y float64 `json:"y"`
	Z float64 `json:"z"`
}

// ============================================================================
// Anomaly Models
// ============================================================================

// KeyAnomaly represents a key anomaly record
type KeyAnomaly struct {
	AnomalyID             string                 `json:"anomaly_id"`
	KeyID                 string                 `json:"key_id"`
	UserID                string                 `json:"user_id"`
	VehicleID             string                 `json:"vehicle_id"`
	AnomalyType           KeyAnomalyType         `json:"anomaly_type"`
	Severity              AnomalySeverity        `json:"severity"`
	Status                AnomalyStatus          `json:"status"`
	DetectionTimestamp    time.Time              `json:"detection_timestamp"`
	DetectedLocation      *Location              `json:"detected_location,omitempty"`
	ConfidenceScore       float64                `json:"confidence_score"`
	KeyUsageCount         *int                   `json:"key_usage_count,omitempty"`
	KeyDistanceFromVehicle *float64              `json:"key_distance_from_vehicle,omitempty"`
	UsageTimeOfDay        *int                   `json:"usage_time_of_day,omitempty"`
	IPAddress             string                 `json:"ip_address,omitempty"`
	DeviceID              string                 `json:"device_id,omitempty"`
	BluetoothSignalStrength *float64             `json:"bluetooth_signal_strength,omitempty"`
	ResolvedAt            *time.Time             `json:"resolved_at,omitempty"`
	ResolvedBy            string                 `json:"resolved_by,omitempty"`
	ResolutionNotes       string                 `json:"resolution_notes,omitempty"`
	RawFeatures           map[string]interface{} `json:"raw_features,omitempty"`
}

// VehicleAnomaly represents a vehicle anomaly record
type VehicleAnomaly struct {
	AnomalyID             string                 `json:"anomaly_id"`
	VehicleID             string                 `json:"vehicle_id"`
	AnomalyType           VehicleAnomalyType     `json:"anomaly_type"`
	Severity              AnomalySeverity        `json:"severity"`
	Status                AnomalyStatus          `json:"status"`
	DetectionTimestamp    time.Time              `json:"detection_timestamp"`
	VehicleLocation       *Location              `json:"vehicle_location,omitempty"`
	ConfidenceScore       float64                `json:"confidence_score"`
	EngineStatus          string                 `json:"engine_status,omitempty"`
	DoorStatus            *DoorStatus            `json:"door_status,omitempty"`
	WindowStatus          *WindowStatus          `json:"window_status,omitempty"`
	IgnitionSource        string                 `json:"ignition_source,omitempty"`
	ExpectedLocation      *Location              `json:"expected_location,omitempty"`
	LocationDeviationMeters *float64             `json:"location_deviation_meters,omitempty"`
	UnauthorizedKeyID     string                 `json:"unauthorized_key_id,omitempty"`
	AccelerometerData     *AccelerometerData     `json:"accelerometer_data,omitempty"`
	GyroscopeData         *GyroscopeData         `json:"gyroscope_data,omitempty"`
	ResolvedAt            *time.Time             `json:"resolved_at,omitempty"`
	ResolvedBy            string                 `json:"resolved_by,omitempty"`
	ResolutionNotes       string                 `json:"resolution_notes,omitempty"`
	RawFeatures           map[string]interface{} `json:"raw_features,omitempty"`
}

// UserBehavior represents a user behavior anomaly record
type UserBehavior struct {
	AnomalyID                     string                 `json:"anomaly_id"`
	UserID                        string                 `json:"user_id"`
	AnomalyType                   UserBehaviorType       `json:"anomaly_type"`
	Severity                      AnomalySeverity        `json:"severity"`
	Status                        AnomalyStatus          `json:"status"`
	DetectionTimestamp            time.Time              `json:"detection_timestamp"`
	ConfidenceScore               float64                `json:"confidence_score"`
	CurrentLocation               Location               `json:"current_location"`
	PreviousLocation              *Location              `json:"previous_location,omitempty"`
	LocationJumpDistanceKm        *float64               `json:"location_jump_distance_km,omitempty"`
	TimeBetweenLocationsMinutes   *float64               `json:"time_between_locations_minutes,omitempty"`
	AccessCount24h                *int                   `json:"access_count_24h,omitempty"`
	UniqueDevicesCount            *int                   `json:"unique_devices_count,omitempty"`
	UniqueLocationsCount          *int                   `json:"unique_locations_count,omitempty"`
	DeviceFingerprint             string                 `json:"device_fingerprint,omitempty"`
	IPAddress                     string                 `json:"ip_address,omitempty"`
	UserAgent                     string                 `json:"user_agent,omitempty"`
	CalculatedVelocityKmh         *float64               `json:"calculated_velocity_kmh,omitempty"`
	MaxPossibleVelocityKmh        float64                `json:"max_possible_velocity_kmh"`
	FailedAuthAttempts            *int                   `json:"failed_auth_attempts,omitempty"`
	FailedAuthTimeWindowMinutes   *int                   `json:"failed_auth_time_window_minutes,omitempty"`
	ResolvedAt                    *time.Time             `json:"resolved_at,omitempty"`
	ResolvedBy                    string                 `json:"resolved_by,omitempty"`
	ResolutionNotes               string                 `json:"resolution_notes,omitempty"`
	RawFeatures                   map[string]interface{} `json:"raw_features,omitempty"`
}

// ============================================================================
// Detection Models
// ============================================================================

// DetectionResult represents a detection result
type DetectionResult struct {
	IsAnomaly       bool            `json:"is_anomaly"`
	AnomalyType     string          `json:"anomaly_type,omitempty"`
	Severity        AnomalySeverity `json:"severity,omitempty"`
	ConfidenceScore float64         `json:"confidence_score"`
	ModelUsed       string          `json:"model_used"`
	FeaturesUsed    []string        `json:"features_used"`
	Explanation     string          `json:"explanation,omitempty"`
	Timestamp       time.Time       `json:"timestamp"`
}

// DetectionData represents generic detection data
type DetectionData struct {
	// Key detection fields
	KeyID                   string              `json:"key_id,omitempty"`
	UserID                  string              `json:"user_id,omitempty"`
	VehicleID               string              `json:"vehicle_id,omitempty"`
	UsageTimeOfDay          int                 `json:"usage_time_of_day,omitempty"`
	KeyDistanceFromVehicle  float64             `json:"key_distance_from_vehicle,omitempty"`
	BluetoothSignalStrength float64             `json:"bluetooth_signal_strength,omitempty"`
	KeyUsageCount           int                 `json:"key_usage_count,omitempty"`
	IsAuthorized            bool                `json:"is_authorized,omitempty"`
	IsSuspended             bool                `json:"is_suspended,omitempty"`
	IsRevoked               bool                `json:"is_revoked,omitempty"`
	
	// Vehicle detection fields
	EngineStatus           string              `json:"engine_status,omitempty"`
	DoorStatus             *DoorStatus         `json:"door_status,omitempty"`
	WindowStatus           *WindowStatus       `json:"window_status,omitempty"`
	IgnitionSource         string              `json:"ignition_source,omitempty"`
	LocationDeviationMeters float64            `json:"location_deviation_meters,omitempty"`
	UnauthorizedKeyID      string              `json:"unauthorized_key_id,omitempty"`
	AccelerometerData      *AccelerometerData  `json:"accelerometer_data,omitempty"`
	
	// User behavior detection fields
	CurrentLocation              Location   `json:"current_location,omitempty"`
	PreviousLocation             *Location  `json:"previous_location,omitempty"`
	LocationJumpDistanceKm       float64    `json:"location_jump_distance_km,omitempty"`
	TimeBetweenLocationsMinutes  float64    `json:"time_between_locations_minutes,omitempty"`
	AccessCount24h               int        `json:"access_count_24h,omitempty"`
	UniqueDevicesCount           int        `json:"unique_devices_count,omitempty"`
	UniqueLocationsCount         int        `json:"unique_locations_count,omitempty"`
	CalculatedVelocityKmh        float64    `json:"calculated_velocity_kmh,omitempty"`
	MaxPossibleVelocityKmh       float64    `json:"max_possible_velocity_kmh,omitempty"`
	FailedAuthAttempts           int        `json:"failed_auth_attempts,omitempty"`
	IPAddress                    string     `json:"ip_address,omitempty"`
	DeviceID                     string     `json:"device_id,omitempty"`
	UserAgent                    string     `json:"user_agent,omitempty"`
	
	// Additional custom fields
	Extra map[string]interface{} `json:"-"`
}

// MarshalJSON implements custom JSON marshaling for DetectionData
type detectionDataJSON DetectionData

func (d DetectionData) MarshalJSON() ([]byte, error) {
	// Marshal the base struct
	base, err := json.Marshal(detectionDataJSON(d))
	if err != nil {
		return nil, err
	}
	
	// Unmarshal to map to merge with Extra
	var result map[string]interface{}
	if err := json.Unmarshal(base, &result); err != nil {
		return nil, err
	}
	
	// Merge Extra fields
	for k, v := range d.Extra {
		result[k] = v
	}
	
	return json.Marshal(result)
}

// BatchDetectionRequest represents a batch detection request
type BatchDetectionRequest struct {
	DetectionType DetectionType          `json:"detection_type"`
	Data          []DetectionData        `json:"data"`
	Threshold     float64                `json:"threshold,omitempty"`
}

// BatchDetectionResponse represents a batch detection response
type BatchDetectionResponse struct {
	ProcessedCount  int               `json:"processed_count"`
	AnomalyCount    int               `json:"anomaly_count"`
	Results         []DetectionResult `json:"results"`
	ProcessingTimeMs float64          `json:"processing_time_ms"`
}

// StreamDetectionResult represents a single result in stream detection
type StreamDetectionResult struct {
	Index  int             `json:"index"`
	Result DetectionResult `json:"result"`
}

// StreamDetectionResponse represents a stream detection response
type StreamDetectionResponse struct {
	Processed    int                     `json:"processed"`
	AnomaliesFound int                   `json:"anomalies_found"`
	AnomalyRate  float64                 `json:"anomaly_rate"`
	Anomalies    []StreamAnomalyInfo     `json:"anomalies"`
	Results      []StreamDetectionResult `json:"results"`
}

// StreamAnomalyInfo represents anomaly info in stream response
type StreamAnomalyInfo struct {
	Index      int     `json:"index"`
	Confidence float64 `json:"confidence"`
	Type       string  `json:"type"`
}

// ============================================================================
// Request/Response Models
// ============================================================================

// HealthCheckResponse represents the health check response
type HealthCheckResponse struct {
	Status    string    `json:"status"`
	Service   string    `json:"service"`
	Timestamp time.Time `json:"timestamp"`
	Version   string    `json:"version"`
}

// DetailedHealthResponse represents detailed health information
type DetailedHealthResponse struct {
	Status         string                 `json:"status"`
	DetectorStatus string                 `json:"detector_status"`
	Stats          map[string]interface{} `json:"stats"`
	Models         map[string]bool        `json:"models"`
}

// AnomalyListResponse represents a list of anomalies response
type AnomalyListResponse struct {
	Total int                      `json:"total"`
	Items []map[string]interface{} `json:"items"`
}

// StatsOverview represents the statistics overview
type StatsOverview struct {
	TotalAnomalies struct {
		Key     int `json:"key"`
		Vehicle int `json:"vehicle"`
		User    int `json:"user"`
		Total   int `json:"total"`
	} `json:"total_anomalies"`
	BySeverity map[string]map[string]int `json:"by_severity"`
	PendingResolution struct {
		Key     int `json:"key"`
		Vehicle int `json:"vehicle"`
		User    int `json:"user"`
	} `json:"pending_resolution"`
}

// AnomalyReport represents an anomaly report
type AnomalyReport struct {
	ReportID                  string                 `json:"report_id"`
	GeneratedAt               time.Time              `json:"generated_at"`
	ReportPeriodStart         time.Time              `json:"report_period_start"`
	ReportPeriodEnd           time.Time              `json:"report_period_end"`
	TotalKeyAnomalies         int                    `json:"total_key_anomalies"`
	TotalVehicleAnomalies     int                    `json:"total_vehicle_anomalies"`
	TotalUserAnomalies        int                    `json:"total_user_anomalies"`
	CriticalCount             int                    `json:"critical_count"`
	HighCount                 int                    `json:"high_count"`
	MediumCount               int                    `json:"medium_count"`
	LowCount                  int                    `json:"low_count"`
	AnomalyTypeDistribution   map[string]int         `json:"anomaly_type_distribution"`
	Anomalies                 []map[string]interface{} `json:"anomalies"`
}

// TrainingRequest represents a model training request
type TrainingRequest struct {
	DetectionType string                 `json:"detection_type"`
	TrainingData  []DetectionData        `json:"training_data"`
	ModelType     ModelType              `json:"model_type,omitempty"`
}

// TrainingResponse represents a model training response
type TrainingResponse struct {
	Message        string `json:"message"`
	ModelType      string `json:"model_type"`
	DetectionType  string `json:"detection_type"`
	TrainingSamples int   `json:"training_samples"`
	FeatureCount   int    `json:"feature_count"`
}

// ModelStatus represents model status information
type ModelStatus struct {
	IsInitialized      bool       `json:"is_initialized"`
	BufferSize         int        `json:"buffer_size"`
}

// ModelsStatusResponse represents the models status response
type ModelsStatusResponse struct {
	Models map[string]ModelStatus `json:"models"`
	Stats  map[string]interface{} `json:"stats"`
}

// UpdateAnomalyStatusRequest represents a request to update anomaly status
type UpdateAnomalyStatusRequest struct {
	Status          AnomalyStatus `json:"status"`
	ResolvedBy      string        `json:"resolved_by,omitempty"`
	ResolutionNotes string        `json:"resolution_notes,omitempty"`
}

// UpdateAnomalyStatusResponse represents the response for updating anomaly status
type UpdateAnomalyStatusResponse struct {
	Message  string                 `json:"message"`
	Anomaly  map[string]interface{} `json:"anomaly"`
}

// APIInfo represents API information
type APIInfo struct {
	Service   string            `json:"service"`
	Version   string            `json:"version"`
	Docs      string            `json:"docs"`
	Endpoints map[string]string `json:"endpoints"`
}

// SaveLoadResponse represents save/load response
type SaveLoadResponse struct {
	SavePath      string `json:"save_path,omitempty"`
	ModelsSaved   int    `json:"models_saved,omitempty"`
	Loaded        bool   `json:"loaded,omitempty"`
	ModelsLoaded  int    `json:"models_loaded,omitempty"`
	Error         string `json:"error,omitempty"`
}