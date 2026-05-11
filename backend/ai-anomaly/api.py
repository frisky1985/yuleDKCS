"""
AI Anomaly Detection API
FastAPI RESTful 接口服务
"""

import os
import uuid
import time
from datetime import datetime, timedelta
from typing import List, Optional, Dict, Any

from fastapi import FastAPI, HTTPException, BackgroundTasks, Depends, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from contextlib import asynccontextmanager

from models import (
    KeyAnomaly, VehicleAnomaly, UserBehavior,
    DetectionResult, AnomalyReport, BatchDetectionRequest, BatchDetectionResponse,
    ModelHealthStatus, AnomalyStatus, AnomalySeverity
)
from detector import AnomalyDetector, get_detector


# 内存存储 (生产环境应使用数据库)
anomaly_storage = {
    'key': [],
    'vehicle': [],
    'user': []
}

# 全局检测器
detector: AnomalyDetector = None


@asynccontextmanager
async def lifespan(app: FastAPI):
    """应用生命周期管理"""
    global detector
    # 启动时初始化
    detector = AnomalyDetector()
    # 尝试加载已有模型
    load_result = detector.load_models()
    print(f"模型加载结果: {load_result}")
    yield
    # 关闭时保存模型
    if detector:
        save_result = detector.save_models()
        print(f"模型保存结果: {save_result}")


app = FastAPI(
    title="AI Anomaly Detection Service",
    description="yuleDKCS AI异常检测引擎 - 支持钥匙、车辆、用户行为异常检测",
    version="1.0.0",
    lifespan=lifespan
)

# CORS 配置
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


def get_current_detector() -> AnomalyDetector:
    """依赖注入获取检测器"""
    if detector is None:
        raise HTTPException(status_code=503, detail="检测服务未初始化")
    return detector


# ==================== 健康检查 ====================

@app.get("/health")
async def health_check():
    """健康检查端点"""
    return {
        "status": "healthy",
        "service": "ai-anomaly-detection",
        "timestamp": datetime.utcnow().isoformat(),
        "version": "1.0.0"
    }


@app.get("/api/v1/health/detailed")
async def detailed_health(detector: AnomalyDetector = Depends(get_current_detector)):
    """详细健康检查"""
    stats = detector.get_stats()
    return {
        "status": "healthy",
        "detector_status": "initialized" if detector else "not_initialized",
        "stats": stats,
        "models": {
            "key": detector.models['key'] is not None,
            "vehicle": detector.models['vehicle'] is not None,
            "user": detector.models['user'] is not None
        }
    }


# ==================== 钥匙异常检测 ====================

@app.post("/api/v1/detect/key", response_model=DetectionResult)
async def detect_key_anomaly(
    data: Dict[str, Any],
    save_anomaly: bool = Query(default=True, description="是否保存异常记录"),
    detector: AnomalyDetector = Depends(get_current_detector)
):
    """
    检测钥匙异常
    
    检测类型:
    - 非法使用 (unauthorized_use)
    - 距离异常 (distance_anomaly)
    - 时间模式异常 (time_pattern_anomaly)
    - 频率异常 (frequency_anomaly)
    """
    result = detector.detect_key_anomaly(data)
    
    # 保存异常记录
    if result.is_anomaly and save_anomaly:
        anomaly = KeyAnomaly(
            anomaly_id=f"key-{uuid.uuid4().hex[:12]}",
            key_id=data.get('key_id', 'unknown'),
            user_id=data.get('user_id', 'unknown'),
            vehicle_id=data.get('vehicle_id', 'unknown'),
            anomaly_type=result.anomaly_type,
            severity=result.severity,
            confidence_score=result.confidence_score,
            detected_location=data.get('location'),
            key_distance_from_vehicle=data.get('key_distance_from_vehicle'),
            usage_time_of_day=data.get('usage_time_of_day'),
            ip_address=data.get('ip_address'),
            device_id=data.get('device_id'),
            bluetooth_signal_strength=data.get('bluetooth_signal_strength'),
            raw_features=data
        )
        anomaly_storage['key'].append(anomaly.dict())
    
    return result


# ==================== 车辆异常检测 ====================

@app.post("/api/v1/detect/vehicle", response_model=DetectionResult)
async def detect_vehicle_anomaly(
    data: Dict[str, Any],
    save_anomaly: bool = Query(default=True, description="是否保存异常记录"),
    detector: AnomalyDetector = Depends(get_current_detector)
):
    """
    检测车辆异常
    
    检测类型:
    - 非法启动 (unauthorized_start)
    - 异常位置 (location_anomaly)
    - 车门状态异常 (door_status_anomaly)
    - 窗户状态异常 (window_status_anomaly)
    - 拖车检测 (towing_detected)
    """
    result = detector.detect_vehicle_anomaly(data)
    
    if result.is_anomaly and save_anomaly:
        anomaly = VehicleAnomaly(
            anomaly_id=f"veh-{uuid.uuid4().hex[:12]}",
            vehicle_id=data.get('vehicle_id', 'unknown'),
            anomaly_type=result.anomaly_type,
            severity=result.severity,
            confidence_score=result.confidence_score,
            vehicle_location=data.get('vehicle_location'),
            engine_status=data.get('engine_status'),
            door_status=data.get('door_status'),
            window_status=data.get('window_status'),
            ignition_source=data.get('ignition_source'),
            location_deviation_meters=data.get('location_deviation_meters'),
            unauthorized_key_id=data.get('unauthorized_key_id'),
            accelerometer_data=data.get('accelerometer_data'),
            raw_features=data
        )
        anomaly_storage['vehicle'].append(anomaly.dict())
    
    return result


# ==================== 用户行为异常检测 ====================

@app.post("/api/v1/detect/user", response_model=DetectionResult)
async def detect_user_behavior(
    data: Dict[str, Any],
    save_anomaly: bool = Query(default=True, description="是否保存异常记录"),
    detector: AnomalyDetector = Depends(get_current_detector)
):
    """
    检测用户行为异常
    
    检测类型:
    - 访问模式异常 (access_pattern_anomaly)
    - 地理位置跳变 (location_jump)
    - 速度异常 (velocity_anomaly)
    - 暴力破解尝试 (brute_force_attempt)
    - 不可能旅行 (impossible_travel)
    """
    result = detector.detect_user_behavior_anomaly(data)
    
    if result.is_anomaly and save_anomaly:
        anomaly = UserBehavior(
            anomaly_id=f"usr-{uuid.uuid4().hex[:12]}",
            user_id=data.get('user_id', 'unknown'),
            anomaly_type=result.anomaly_type,
            severity=result.severity,
            confidence_score=result.confidence_score,
            current_location=data.get('current_location', {}),
            previous_location=data.get('previous_location'),
            location_jump_distance_km=data.get('location_jump_distance_km'),
            time_between_locations_minutes=data.get('time_between_locations_minutes'),
            access_count_24h=data.get('access_count_24h'),
            unique_devices_count=data.get('unique_devices_count'),
            unique_locations_count=data.get('unique_locations_count'),
            calculated_velocity_kmh=data.get('calculated_velocity_kmh'),
            failed_auth_attempts=data.get('failed_auth_attempts'),
            ip_address=data.get('ip_address'),
            user_agent=data.get('user_agent'),
            raw_features=data
        )
        anomaly_storage['user'].append(anomaly.dict())
    
    return result


# ==================== 批量检测 ====================

@app.post("/api/v1/detect/batch", response_model=BatchDetectionResponse)
async def batch_detect(
    request: BatchDetectionRequest,
    detector: AnomalyDetector = Depends(get_current_detector)
):
    """批量异常检测"""
    start_time = time.time()
    
    if request.detection_type not in ['key', 'vehicle', 'user']:
        raise HTTPException(status_code=400, detail="无效的检测类型")
    
    results = detector.batch_detect(request.detection_type, request.data)
    
    # 过滤低于阈值的异常
    filtered_results = [
        r for r in results 
        if not r.is_anomaly or r.confidence_score >= request.threshold
    ]
    
    anomaly_count = sum(1 for r in filtered_results if r.is_anomaly)
    processing_time = (time.time() - start_time) * 1000
    
    return BatchDetectionResponse(
        processed_count=len(request.data),
        anomaly_count=anomaly_count,
        results=filtered_results,
        processing_time_ms=round(processing_time, 2)
    )


# ==================== 异常记录查询 ====================

@app.get("/api/v1/anomalies/key")
async def get_key_anomalies(
    severity: Optional[AnomalySeverity] = None,
    status: Optional[AnomalyStatus] = None,
    limit: int = Query(default=50, le=100),
    offset: int = 0
):
    """获取钥匙异常记录"""
    anomalies = anomaly_storage['key']
    
    if severity:
        anomalies = [a for a in anomalies if a.get('severity') == severity]
    if status:
        anomalies = [a for a in anomalies if a.get('status') == status]
    
    # 按时间倒序
    anomalies = sorted(anomalies, key=lambda x: x.get('detection_timestamp', ''), reverse=True)
    
    return {
        "total": len(anomalies),
        "items": anomalies[offset:offset + limit]
    }


@app.get("/api/v1/anomalies/vehicle")
async def get_vehicle_anomalies(
    severity: Optional[AnomalySeverity] = None,
    status: Optional[AnomalyStatus] = None,
    limit: int = Query(default=50, le=100),
    offset: int = 0
):
    """获取车辆异常记录"""
    anomalies = anomaly_storage['vehicle']
    
    if severity:
        anomalies = [a for a in anomalies if a.get('severity') == severity]
    if status:
        anomalies = [a for a in anomalies if a.get('status') == status]
    
    anomalies = sorted(anomalies, key=lambda x: x.get('detection_timestamp', ''), reverse=True)
    
    return {
        "total": len(anomalies),
        "items": anomalies[offset:offset + limit]
    }


@app.get("/api/v1/anomalies/user")
async def get_user_anomalies(
    severity: Optional[AnomalySeverity] = None,
    status: Optional[AnomalyStatus] = None,
    limit: int = Query(default=50, le=100),
    offset: int = 0
):
    """获取用户行为异常记录"""
    anomalies = anomaly_storage['user']
    
    if severity:
        anomalies = [a for a in anomalies if a.get('severity') == severity]
    if status:
        anomalies = [a for a in anomalies if a.get('status') == status]
    
    anomalies = sorted(anomalies, key=lambda x: x.get('detection_timestamp', ''), reverse=True)
    
    return {
        "total": len(anomalies),
        "items": anomalies[offset:offset + limit]
    }


# ==================== 异常记录管理 ====================

@app.patch("/api/v1/anomalies/{anomaly_type}/{anomaly_id}/status")
async def update_anomaly_status(
    anomaly_type: str,
    anomaly_id: str,
    status: AnomalyStatus,
    resolved_by: Optional[str] = None,
    resolution_notes: Optional[str] = None
):
    """更新异常状态"""
    if anomaly_type not in anomaly_storage:
        raise HTTPException(status_code=404, detail="异常类型不存在")
    
    for anomaly in anomaly_storage[anomaly_type]:
        if anomaly.get('anomaly_id') == anomaly_id:
            anomaly['status'] = status
            anomaly['resolved_at'] = datetime.utcnow().isoformat()
            if resolved_by:
                anomaly['resolved_by'] = resolved_by
            if resolution_notes:
                anomaly['resolution_notes'] = resolution_notes
            return {"message": "状态更新成功", "anomaly": anomaly}
    
    raise HTTPException(status_code=404, detail="异常记录不存在")


# ==================== 统计与报告 ====================

@app.get("/api/v1/stats/overview")
async def get_stats_overview():
    """获取统计概览"""
    key_anomalies = anomaly_storage['key']
    vehicle_anomalies = anomaly_storage['vehicle']
    user_anomalies = anomaly_storage['user']
    
    def count_by_severity(anomalies):
        counts = {'critical': 0, 'high': 0, 'medium': 0, 'low': 0}
        for a in anomalies:
            sev = a.get('severity', 'low')
            if sev in counts:
                counts[sev] += 1
        return counts
    
    return {
        "total_anomalies": {
            "key": len(key_anomalies),
            "vehicle": len(vehicle_anomalies),
            "user": len(user_anomalies),
            "total": len(key_anomalies) + len(vehicle_anomalies) + len(user_anomalies)
        },
        "by_severity": {
            "key": count_by_severity(key_anomalies),
            "vehicle": count_by_severity(vehicle_anomalies),
            "user": count_by_severity(user_anomalies)
        },
        "pending_resolution": {
            "key": len([a for a in key_anomalies if a.get('status') == 'detected']),
            "vehicle": len([a for a in vehicle_anomalies if a.get('status') == 'detected']),
            "user": len([a for a in user_anomalies if a.get('status') == 'detected'])
        }
    }


@app.post("/api/v1/reports/generate")
async def generate_report(
    start_date: datetime,
    end_date: datetime,
    background_tasks: BackgroundTasks
):
    """生成异常报告"""
    report_id = f"rpt-{uuid.uuid4().hex[:12]}"
    
    # 过滤时间段内的异常
    def filter_by_date(anomalies, start, end):
        return [
            a for a in anomalies 
            if start <= datetime.fromisoformat(a.get('detection_timestamp', '2000-01-01').replace('Z', '+00:00').replace('+00:00', '')) <= end
        ]
    
    key_items = filter_by_date(anomaly_storage['key'], start_date, end_date)
    vehicle_items = filter_by_date(anomaly_storage['vehicle'], start_date, end_date)
    user_items = filter_by_date(anomaly_storage['user'], start_date, end_date)
    
    all_items = key_items + vehicle_items + user_items
    
    # 统计
    severity_counts = {'critical': 0, 'high': 0, 'medium': 0, 'low': 0}
    type_distribution = {}
    
    for item in all_items:
        sev = item.get('severity', 'low')
        severity_counts[sev] = severity_counts.get(sev, 0) + 1
        
        anomaly_type = item.get('anomaly_type', 'unknown')
        type_distribution[anomaly_type] = type_distribution.get(anomaly_type, 0) + 1
    
    report = {
        "report_id": report_id,
        "generated_at": datetime.utcnow().isoformat(),
        "report_period_start": start_date.isoformat(),
        "report_period_end": end_date.isoformat(),
        "total_key_anomalies": len(key_items),
        "total_vehicle_anomalies": len(vehicle_items),
        "total_user_anomalies": len(user_items),
        "critical_count": severity_counts['critical'],
        "high_count": severity_counts['high'],
        "medium_count": severity_counts['medium'],
        "low_count": severity_counts['low'],
        "anomaly_type_distribution": type_distribution,
        "anomalies": all_items[:100]  # 最多100条
    }
    
    return report


# ==================== 模型训练与管理 ====================

@app.post("/api/v1/models/train")
async def train_model(
    detection_type: str,
    training_data: List[Dict[str, Any]],
    model_type: str = Query(default='isolation_forest', 
                            enum=['isolation_forest', 'one_class_svm']),
    detector: AnomalyDetector = Depends(get_current_detector)
):
    """
    训练异常检测模型
    
    detection_type: key, vehicle, user
    model_type: isolation_forest, one_class_svm
    """
    if detection_type not in ['key', 'vehicle', 'user']:
        raise HTTPException(status_code=400, detail="无效的检测类型")
    
    if len(training_data) < 10:
        raise HTTPException(status_code=400, detail="训练数据至少需要10条")
    
    try:
        result = detector.train(detection_type, training_data, model_type)
        return {
            "message": "模型训练成功",
            "result": result
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"训练失败: {str(e)}")


@app.post("/api/v1/models/save")
async def save_models(
    save_path: Optional[str] = None,
    detector: AnomalyDetector = Depends(get_current_detector)
):
    """保存模型到磁盘"""
    result = detector.save_models(save_path)
    return result


@app.post("/api/v1/models/load")
async def load_models(
    load_path: Optional[str] = None,
    detector: AnomalyDetector = Depends(get_current_detector)
):
    """从磁盘加载模型"""
    result = detector.load_models(load_path)
    if not result.get('loaded'):
        raise HTTPException(status_code=500, detail=result.get('error', '加载失败'))
    return result


@app.get("/api/v1/models/status")
async def get_model_status(
    detector: AnomalyDetector = Depends(get_current_detector)
):
    """获取模型状态"""
    stats = detector.get_stats()
    
    return {
        "models": {
            "key": {
                "is_initialized": detector.models['key'] is not None,
                "buffer_size": len(detector.data_buffer['key'])
            },
            "vehicle": {
                "is_initialized": detector.models['vehicle'] is not None,
                "buffer_size": len(detector.data_buffer['vehicle'])
            },
            "user": {
                "is_initialized": detector.models['user'] is not None,
                "buffer_size": len(detector.data_buffer['user'])
            }
        },
        "stats": stats
    }


# ==================== 实时流检测 (WebSocket预留) ====================

@app.post("/api/v1/detect/stream/simulate")
async def simulate_stream_detection(
    data_points: List[Dict[str, Any]],
    detection_type: str,
    detector: AnomalyDetector = Depends(get_current_detector)
):
    """模拟流式检测 (批量输入，实时处理)"""
    results = []
    anomalies_found = []
    
    for i, data in enumerate(data_points):
        if detection_type == 'key':
            result = detector.detect_key_anomaly(data)
        elif detection_type == 'vehicle':
            result = detector.detect_vehicle_anomaly(data)
        else:
            result = detector.detect_user_behavior_anomaly(data)
        
        results.append({
            "index": i,
            "result": result
        })
        
        if result.is_anomaly:
            anomalies_found.append({
                "index": i,
                "confidence": result.confidence_score,
                "type": result.anomaly_type
            })
    
    return {
        "processed": len(data_points),
        "anomalies_found": len(anomalies_found),
        "anomaly_rate": len(anomalies_found) / len(data_points) if data_points else 0,
        "anomalies": anomalies_found,
        "results": results
    }


# ==================== 文档与帮助 ====================

@app.get("/")
async def root():
    """根路径 - API信息"""
    return {
        "service": "AI Anomaly Detection API",
        "version": "1.0.0",
        "docs": "/docs",
        "endpoints": {
            "health": "/health",
            "detect_key": "POST /api/v1/detect/key",
            "detect_vehicle": "POST /api/v1/detect/vehicle",
            "detect_user": "POST /api/v1/detect/user",
            "batch_detect": "POST /api/v1/detect/batch",
            "anomalies": "/api/v1/anomalies/{key|vehicle|user}",
            "stats": "/api/v1/stats/overview",
            "train": "POST /api/v1/models/train"
        }
    }


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
