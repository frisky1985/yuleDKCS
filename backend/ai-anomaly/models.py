"""
AI Anomaly Detection Models
数据模型定义 - 钥匙异常、车辆异常、用户行为异常
"""

from datetime import datetime
from typing import Optional, Dict, Any, List
from enum import Enum
from pydantic import BaseModel, Field


class AnomalySeverity(str, Enum):
    """异常严重程度"""
    LOW = "low"           # 低风险
    MEDIUM = "medium"     # 中等风险
    HIGH = "high"         # 高风险
    CRITICAL = "critical" # 严重风险


class AnomalyStatus(str, Enum):
    """异常状态"""
    DETECTED = "detected"     # 已检测
    CONFIRMED = "confirmed"   # 已确认
    RESOLVED = "resolved"     # 已解决
    FALSE_POSITIVE = "false_positive"  # 误报


class KeyAnomalyType(str, Enum):
    """钥匙异常类型"""
    UNAUTHORIZED_USE = "unauthorized_use"      # 非法使用
    DISTANCE_ANOMALY = "distance_anomaly"      # 距离异常
    TIME_PATTERN_ANOMALY = "time_pattern_anomaly"  # 时间模式异常
    FREQUENCY_ANOMALY = "frequency_anomaly"    # 频率异常
    DUPLICATE_ACCESS = "duplicate_access"      # 重复访问
    SUSPENDED_KEY_USED = "suspended_key_used"  # 使用已暂停钥匙


class VehicleAnomalyType(str, Enum):
    """车辆异常类型"""
    UNAUTHORIZED_START = "unauthorized_start"  # 非法启动
    LOCATION_ANOMALY = "location_anomaly"      # 异常位置
    DOOR_STATUS_ANOMALY = "door_status_anomaly"    # 车门状态异常
    WINDOW_STATUS_ANOMALY = "window_status_anomaly"  # 窗户状态异常
    TOWING_DETECTED = "towing_detected"        # 拖车检测
    TAMPERING_DETECTED = "tampering_detected"  # 篡改检测


class UserBehaviorType(str, Enum):
    """用户行为异常类型"""
    ACCESS_PATTERN_ANOMALY = "access_pattern_anomaly"  # 访问模式异常
    LOCATION_JUMP = "location_jump"              # 地理位置跳变
    VELOCITY_ANOMALY = "velocity_anomaly"        # 速度异常
    DEVICE_SWITCH_ANOMALY = "device_switch_anomaly"  # 设备切换异常
    BRUTE_FORCE_ATTEMPT = "brute_force_attempt"  # 暴力破解尝试
    IMPOSSIBLE_TRAVEL = "impossible_travel"      # 不可能旅行


class KeyAnomaly(BaseModel):
    """钥匙异常数据模型"""
    anomaly_id: str = Field(..., description="异常唯一标识")
    key_id: str = Field(..., description="数字钥匙ID")
    user_id: str = Field(..., description="用户ID")
    vehicle_id: str = Field(..., description="车辆ID")
    anomaly_type: KeyAnomalyType = Field(..., description="异常类型")
    severity: AnomalySeverity = Field(..., description="严重程度")
    status: AnomalyStatus = Field(default=AnomalyStatus.DETECTED, description="状态")
    
    # 检测详情
    detection_timestamp: datetime = Field(default_factory=datetime.utcnow)
    detected_location: Optional[Dict[str, float]] = Field(None, description="检测位置")
    confidence_score: float = Field(..., ge=0.0, le=1.0, description="置信度分数")
    
    # 钥匙使用详情
    key_usage_count: Optional[int] = Field(None, description="钥匙使用次数")
    key_distance_from_vehicle: Optional[float] = Field(None, description="钥匙与车辆距离")
    usage_time_of_day: Optional[int] = Field(None, ge=0, le=23, description="使用时段")
    
    # 上下文信息
    ip_address: Optional[str] = Field(None, description="IP地址")
    device_id: Optional[str] = Field(None, description="设备ID")
    bluetooth_signal_strength: Optional[float] = Field(None, description="蓝牙信号强度")
    
    # 处理信息
    resolved_at: Optional[datetime] = Field(None, description="解决时间")
    resolved_by: Optional[str] = Field(None, description="解决人")
    resolution_notes: Optional[str] = Field(None, description="解决备注")
    
    # 原始数据
    raw_features: Dict[str, Any] = Field(default_factory=dict, description="原始特征数据")
    
    class Config:
        json_schema_extra = {
            "example": {
                "anomaly_id": "anom-12345",
                "key_id": "key-67890",
                "user_id": "user-11111",
                "vehicle_id": "veh-22222",
                "anomaly_type": "distance_anomaly",
                "severity": "high",
                "status": "detected",
                "detection_timestamp": "2024-01-15T08:30:00Z",
                "detected_location": {"lat": 39.9042, "lng": 116.4074},
                "confidence_score": 0.87,
                "key_distance_from_vehicle": 500.0,
                "usage_time_of_day": 3
            }
        }


class VehicleAnomaly(BaseModel):
    """车辆异常数据模型"""
    anomaly_id: str = Field(..., description="异常唯一标识")
    vehicle_id: str = Field(..., description="车辆ID")
    anomaly_type: VehicleAnomalyType = Field(..., description="异常类型")
    severity: AnomalySeverity = Field(..., description="严重程度")
    status: AnomalyStatus = Field(default=AnomalyStatus.DETECTED, description="状态")
    
    # 检测详情
    detection_timestamp: datetime = Field(default_factory=datetime.utcnow)
    vehicle_location: Optional[Dict[str, float]] = Field(None, description="车辆位置")
    confidence_score: float = Field(..., ge=0.0, le=1.0, description="置信度分数")
    
    # 车辆状态
    engine_status: Optional[str] = Field(None, description="引擎状态")
    door_status: Optional[Dict[str, str]] = Field(None, description="车门状态")
    window_status: Optional[Dict[str, str]] = Field(None, description="窗户状态")
    ignition_source: Optional[str] = Field(None, description="点火来源")
    
    # 异常详情
    expected_location: Optional[Dict[str, float]] = Field(None, description="预期位置")
    location_deviation_meters: Optional[float] = Field(None, description="位置偏差")
    unauthorized_key_id: Optional[str] = Field(None, description="未授权钥匙ID")
    
    # 传感器数据
    accelerometer_data: Optional[Dict[str, float]] = Field(None, description="加速度计数据")
    gyroscope_data: Optional[Dict[str, float]] = Field(None, description="陀螺仪数据")
    
    # 处理信息
    resolved_at: Optional[datetime] = Field(None, description="解决时间")
    resolved_by: Optional[str] = Field(None, description="解决人")
    resolution_notes: Optional[str] = Field(None, description="解决备注")
    
    # 原始数据
    raw_features: Dict[str, Any] = Field(default_factory=dict, description="原始特征数据")
    
    class Config:
        json_schema_extra = {
            "example": {
                "anomaly_id": "anom-54321",
                "vehicle_id": "veh-22222",
                "anomaly_type": "unauthorized_start",
                "severity": "critical",
                "status": "detected",
                "detection_timestamp": "2024-01-15T10:15:00Z",
                "vehicle_location": {"lat": 39.9042, "lng": 116.4074},
                "confidence_score": 0.95,
                "engine_status": "running",
                "ignition_source": "unknown_key"
            }
        }


class UserBehavior(BaseModel):
    """用户行为异常数据模型"""
    anomaly_id: str = Field(..., description="异常唯一标识")
    user_id: str = Field(..., description="用户ID")
    anomaly_type: UserBehaviorType = Field(..., description="异常类型")
    severity: AnomalySeverity = Field(..., description="严重程度")
    status: AnomalyStatus = Field(default=AnomalyStatus.DETECTED, description="状态")
    
    # 检测详情
    detection_timestamp: datetime = Field(default_factory=datetime.utcnow)
    confidence_score: float = Field(..., ge=0.0, le=1.0, description="置信度分数")
    
    # 位置信息
    current_location: Dict[str, float] = Field(..., description="当前位置")
    previous_location: Optional[Dict[str, float]] = Field(None, description="上一位置")
    location_jump_distance_km: Optional[float] = Field(None, description="位置跳变距离")
    time_between_locations_minutes: Optional[float] = Field(None, description="位置间时间差")
    
    # 访问模式
    access_count_24h: Optional[int] = Field(None, description="24小时访问次数")
    unique_devices_count: Optional[int] = Field(None, description="设备数量")
    unique_locations_count: Optional[int] = Field(None, description="位置数量")
    
    # 设备信息
    device_fingerprint: Optional[str] = Field(None, description="设备指纹")
    ip_address: Optional[str] = Field(None, description="IP地址")
    user_agent: Optional[str] = Field(None, description="用户代理")
    
    # 速度计算
    calculated_velocity_kmh: Optional[float] = Field(None, description="计算速度")
    max_possible_velocity_kmh: float = Field(default=900.0, description="最大可能速度")
    
    # 失败尝试
    failed_auth_attempts: Optional[int] = Field(None, description="认证失败次数")
    failed_auth_time_window_minutes: Optional[int] = Field(None, description="失败时间窗口")
    
    # 处理信息
    resolved_at: Optional[datetime] = Field(None, description="解决时间")
    resolved_by: Optional[str] = Field(None, description="解决人")
    resolution_notes: Optional[str] = Field(None, description="解决备注")
    
    # 原始数据
    raw_features: Dict[str, Any] = Field(default_factory=dict, description="原始特征数据")
    
    class Config:
        json_schema_extra = {
            "example": {
                "anomaly_id": "anom-98765",
                "user_id": "user-11111",
                "anomaly_type": "impossible_travel",
                "severity": "high",
                "status": "detected",
                "detection_timestamp": "2024-01-15T14:20:00Z",
                "confidence_score": 0.92,
                "current_location": {"lat": 48.8566, "lng": 2.3522},
                "previous_location": {"lat": 39.9042, "lng": 116.4074},
                "location_jump_distance_km": 8200.0,
                "time_between_locations_minutes": 30.0,
                "calculated_velocity_kmh": 16400.0
            }
        }


class DetectionResult(BaseModel):
    """检测结果通用模型"""
    is_anomaly: bool = Field(..., description="是否异常")
    anomaly_type: Optional[str] = Field(None, description="异常类型")
    severity: Optional[AnomalySeverity] = Field(None, description="严重程度")
    confidence_score: float = Field(..., ge=0.0, le=1.0, description="置信度")
    model_used: str = Field(..., description="使用的模型")
    features_used: List[str] = Field(default_factory=list, description="使用的特征")
    explanation: Optional[str] = Field(None, description="异常解释")
    timestamp: datetime = Field(default_factory=datetime.utcnow)


class AnomalyReport(BaseModel):
    """异常报告模型"""
    report_id: str = Field(..., description="报告ID")
    generated_at: datetime = Field(default_factory=datetime.utcnow)
    report_period_start: datetime = Field(..., description="报告开始时间")
    report_period_end: datetime = Field(..., description="报告结束时间")
    
    # 统计信息
    total_key_anomalies: int = Field(default=0)
    total_vehicle_anomalies: int = Field(default=0)
    total_user_anomalies: int = Field(default=0)
    
    # 按严重程度统计
    critical_count: int = Field(default=0)
    high_count: int = Field(default=0)
    medium_count: int = Field(default=0)
    low_count: int = Field(default=0)
    
    # 按类型统计
    anomaly_type_distribution: Dict[str, int] = Field(default_factory=dict)
    
    # 详细列表
    anomalies: List[Dict[str, Any]] = Field(default_factory=list)
    
    # 摘要
    summary: Optional[str] = Field(None, description="报告摘要")


class BatchDetectionRequest(BaseModel):
    """批量检测请求模型"""
    detection_type: str = Field(..., description="检测类型: key/vehicle/user")
    data: List[Dict[str, Any]] = Field(..., description="批量数据")
    threshold: Optional[float] = Field(default=0.5, ge=0.0, le=1.0, description="检测阈值")


class BatchDetectionResponse(BaseModel):
    """批量检测响应模型"""
    processed_count: int = Field(..., description="处理数量")
    anomaly_count: int = Field(..., description="异常数量")
    results: List[DetectionResult] = Field(default_factory=list)
    processing_time_ms: float = Field(..., description="处理时间(毫秒)")


class ModelHealthStatus(BaseModel):
    """模型健康状态"""
    model_name: str = Field(..., description="模型名称")
    is_healthy: bool = Field(..., description="是否健康")
    last_trained_at: Optional[datetime] = Field(None, description="最后训练时间")
    training_samples_count: Optional[int] = Field(None, description="训练样本数")
    accuracy_score: Optional[float] = Field(None, description="准确率")
    false_positive_rate: Optional[float] = Field(None, description="误报率")
    status_message: str = Field(..., description="状态消息")
