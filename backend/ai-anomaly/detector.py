"""
AI Anomaly Detection Engine
实时异常检测引擎 - 支持 IsolationForest, One-Class SVM
"""

import numpy as np
import pandas as pd
from typing import Dict, List, Optional, Tuple, Any, Union
from datetime import datetime, timedelta
from collections import deque
import asyncio
from concurrent.futures import ThreadPoolExecutor
import joblib
import os
import yaml

from sklearn.ensemble import IsolationForest
from sklearn.svm import OneClassSVM
from sklearn.preprocessing import StandardScaler, MinMaxScaler
from sklearn.decomposition import PCA
from sklearn.cluster import DBSCAN
import warnings
warnings.filterwarnings('ignore')

from models import (
    KeyAnomaly, VehicleAnomaly, UserBehavior,
    KeyAnomalyType, VehicleAnomalyType, UserBehaviorType,
    AnomalySeverity, DetectionResult
)


class FeatureExtractor:
    """特征提取器"""
    
    @staticmethod
    def extract_key_features(data: Dict[str, Any]) -> np.ndarray:
        """提取钥匙相关特征"""
        features = []
        
        # 时间特征
        hour = data.get('usage_time_of_day', datetime.now().hour)
        features.append(hour / 24.0)  # 归一化到0-1
        features.append(np.sin(2 * np.pi * hour / 24))  # 周期性特征
        features.append(np.cos(2 * np.pi * hour / 24))
        
        # 距离特征
        distance = data.get('key_distance_from_vehicle', 0)
        features.append(min(distance / 1000.0, 1.0))  # 归一化到1km
        
        # 信号强度特征
        signal_strength = data.get('bluetooth_signal_strength', -50)
        features.append((signal_strength + 100) / 100.0)  # 归一化
        
        # 使用频率特征
        usage_count = data.get('key_usage_count', 0)
        features.append(min(usage_count / 100.0, 1.0))
        
        # 认证状态
        is_authorized = 1.0 if data.get('is_authorized', True) else 0.0
        features.append(is_authorized)
        
        # 钥匙状态
        is_suspended = 1.0 if data.get('is_suspended', False) else 0.0
        features.append(is_suspended)
        is_revoked = 1.0 if data.get('is_revoked', False) else 0.0
        features.append(is_revoked)
        
        return np.array(features).reshape(1, -1)
    
    @staticmethod
    def extract_vehicle_features(data: Dict[str, Any]) -> np.ndarray:
        """提取车辆相关特征"""
        features = []
        
        # 引擎状态
        engine_running = 1.0 if data.get('engine_status') == 'running' else 0.0
        features.append(engine_running)
        
        # 车门状态异常
        door_status = data.get('door_status', {})
        door_anomaly = 0.0
        for door, status in door_status.items():
            if status not in ['locked', 'closed']:
                door_anomaly = 1.0
                break
        features.append(door_anomaly)
        
        # 窗户状态异常
        window_status = data.get('window_status', {})
        window_anomaly = 0.0
        for window, status in window_status.items():
            if status not in ['closed', 'up']:
                window_anomaly = 1.0
                break
        features.append(window_anomaly)
        
        # 位置偏差
        location_deviation = data.get('location_deviation_meters', 0)
        features.append(min(location_deviation / 5000.0, 1.0))
        
        # 未授权访问
        unauthorized_access = 1.0 if data.get('unauthorized_key_id') else 0.0
        features.append(unauthorized_access)
        
        # 拖车检测特征
        accelerometer = data.get('accelerometer_data', {})
        accel_magnitude = np.sqrt(
            accelerometer.get('x', 0)**2 + 
            accelerometer.get('y', 0)**2 + 
            accelerometer.get('z', 0)**2
        )
        features.append(min(accel_magnitude / 20.0, 1.0))
        
        # 点火来源合法性
        ignition_legal = 1.0 if data.get('ignition_source') in ['authorized_key', 'fob'] else 0.0
        features.append(ignition_legal)
        
        return np.array(features).reshape(1, -1)
    
    @staticmethod
    def extract_user_behavior_features(data: Dict[str, Any]) -> np.ndarray:
        """提取用户行为特征"""
        features = []
        
        # 访问频率特征
        access_count = data.get('access_count_24h', 0)
        features.append(min(access_count / 50.0, 1.0))
        
        # 设备多样性
        unique_devices = data.get('unique_devices_count', 1)
        features.append(min(unique_devices / 5.0, 1.0))
        
        # 位置多样性
        unique_locations = data.get('unique_locations_count', 1)
        features.append(min(unique_locations / 10.0, 1.0))
        
        # 失败认证尝试
        failed_attempts = data.get('failed_auth_attempts', 0)
        features.append(min(failed_attempts / 10.0, 1.0))
        
        # 地理位置跳变特征
        jump_distance = data.get('location_jump_distance_km', 0)
        features.append(min(jump_distance / 10000.0, 1.0))
        
        # 时间差特征
        time_diff = data.get('time_between_locations_minutes', 0)
        features.append(min(time_diff / 1440.0, 1.0))  # 归一化到1天
        
        # 速度异常
        velocity = data.get('calculated_velocity_kmh', 0)
        max_velocity = data.get('max_possible_velocity_kmh', 900)
        velocity_ratio = velocity / max_velocity if max_velocity > 0 else 0
        features.append(min(velocity_ratio, 1.0))
        
        # 不可能旅行检测
        impossible_travel = 1.0 if velocity > max_velocity else 0.0
        features.append(impossible_travel)
        
        return np.array(features).reshape(1, -1)


class AnomalyDetector:
    """异常检测引擎"""
    
    def __init__(self, config_path: str = "config.yaml"):
        self.config = self._load_config(config_path)
        self.feature_extractor = FeatureExtractor()
        
        # 模型存储
        self.models = {
            'key': None,
            'vehicle': None,
            'user': None
        }
        self.scalers = {
            'key': StandardScaler(),
            'vehicle': StandardScaler(),
            'user': StandardScaler()
        }
        
        # 历史数据缓存 (用于增量学习)
        self.data_buffer = {
            'key': deque(maxlen=self.config.get('buffer_size', 10000)),
            'vehicle': deque(maxlen=self.config.get('buffer_size', 10000)),
            'user': deque(maxlen=self.config.get('buffer_size', 10000))
        }
        
        # 统计信息
        self.detection_stats = {
            'total_detections': 0,
            'anomalies_detected': 0,
            'false_positives': 0
        }
        
        # 线程池用于异步处理
        self.executor = ThreadPoolExecutor(max_workers=4)
        
        # 初始化模型
        self._init_models()
    
    def _load_config(self, config_path: str) -> Dict:
        """加载配置文件"""
        if os.path.exists(config_path):
            with open(config_path, 'r') as f:
                return yaml.safe_load(f)
        return self._default_config()
    
    def _default_config(self) -> Dict:
        """默认配置"""
        return {
            'isolation_forest': {
                'n_estimators': 100,
                'contamination': 0.1,
                'random_state': 42
            },
            'one_class_svm': {
                'kernel': 'rbf',
                'gamma': 'scale',
                'nu': 0.1
            },
            'buffer_size': 10000,
            'detection_threshold': 0.5,
            'model_save_path': './models'
        }
    
    def _init_models(self):
        """初始化检测模型"""
        # Isolation Forest 配置
        if_config = self.config.get('isolation_forest', {})
        
        # One-Class SVM 配置
        svm_config = self.config.get('one_class_svm', {})
        
        for detection_type in ['key', 'vehicle', 'user']:
            self.models[detection_type] = {
                'isolation_forest': IsolationForest(
                    n_estimators=if_config.get('n_estimators', 100),
                    contamination=if_config.get('contamination', 0.1),
                    random_state=if_config.get('random_state', 42)
                ),
                'one_class_svm': OneClassSVM(
                    kernel=svm_config.get('kernel', 'rbf'),
                    gamma=svm_config.get('gamma', 'scale'),
                    nu=svm_config.get('nu', 0.1)
                )
            }
    
    def train(self, detection_type: str, training_data: List[Dict[str, Any]], 
              model_type: str = 'isolation_forest'):
        """训练模型"""
        if detection_type not in self.models:
            raise ValueError(f"Unknown detection type: {detection_type}")
        
        # 提取特征
        features_list = []
        for data in training_data:
            if detection_type == 'key':
                features = self.feature_extractor.extract_key_features(data)
            elif detection_type == 'vehicle':
                features = self.feature_extractor.extract_vehicle_features(data)
            else:
                features = self.feature_extractor.extract_user_behavior_features(data)
            features_list.append(features[0])
        
        X = np.array(features_list)
        
        # 标准化
        X_scaled = self.scalers[detection_type].fit_transform(X)
        
        # 训练模型
        model = self.models[detection_type][model_type]
        model.fit(X_scaled)
        
        # 保存训练数据到缓存
        self.data_buffer[detection_type].extend(X_scaled)
        
        return {
            'model_type': model_type,
            'detection_type': detection_type,
            'training_samples': len(training_data),
            'feature_count': X.shape[1]
        }
    
    def detect_key_anomaly(self, data: Dict[str, Any]) -> DetectionResult:
        """检测钥匙异常"""
        features = self.feature_extractor.extract_key_features(data)
        return self._detect('key', features, data)
    
    def detect_vehicle_anomaly(self, data: Dict[str, Any]) -> DetectionResult:
        """检测车辆异常"""
        features = self.feature_extractor.extract_vehicle_features(data)
        return self._detect('vehicle', features, data)
    
    def detect_user_behavior_anomaly(self, data: Dict[str, Any]) -> DetectionResult:
        """检测用户行为异常"""
        features = self.feature_extractor.extract_user_behavior_features(data)
        return self._detect('user', features, data)
    
    def _detect(self, detection_type: str, features: np.ndarray, 
                raw_data: Dict[str, Any]) -> DetectionResult:
        """执行检测"""
        self.detection_stats['total_detections'] += 1
        
        # 标准化特征
        try:
            features_scaled = self.scalers[detection_type].transform(features)
        except:
            # 如果scaler未fit，直接使用原始特征
            features_scaled = features
        
        # 使用集成检测 (Isolation Forest + One-Class SVM)
        if_model = self.models[detection_type]['isolation_forest']
        svm_model = self.models[detection_type]['one_class_svm']
        
        scores = []
        
        # Isolation Forest 预测
        try:
            if_score = if_model.decision_function(features_scaled)[0]
            scores.append(('isolation_forest', if_score))
        except:
            if_score = 0
        
        # One-Class SVM 预测
        try:
            svm_score = svm_model.decision_function(features_scaled)[0]
            scores.append(('one_class_svm', svm_score))
        except:
            svm_score = 0
        
        # 计算综合异常分数 (归一化到0-1)
        # Isolation Forest: 负值表示异常
        if_normalized = 1.0 - (if_score + 0.5) if if_score is not None else 0.5
        if_normalized = max(0.0, min(1.0, if_normalized))
        
        # One-Class SVM: 负值表示异常
        svm_normalized = 1.0 - (svm_score + 0.5) if svm_score is not None else 0.5
        svm_normalized = max(0.0, min(1.0, svm_normalized))
        
        # 加权平均
        confidence_score = (if_normalized * 0.6 + svm_normalized * 0.4)
        
        # 规则检测 (基于业务规则)
        rule_score, rule_type = self._rule_based_detection(detection_type, raw_data)
        
        # 综合判断
        is_anomaly = confidence_score > self.config.get('detection_threshold', 0.5) or rule_score > 0.8
        
        if rule_score > confidence_score:
            confidence_score = rule_score
        
        # 确定异常类型和严重程度
        anomaly_type, severity = self._classify_anomaly(
            detection_type, raw_data, confidence_score, rule_type
        )
        
        if is_anomaly:
            self.detection_stats['anomalies_detected'] += 1
        
        return DetectionResult(
            is_anomaly=is_anomaly,
            anomaly_type=anomaly_type,
            severity=severity,
            confidence_score=round(confidence_score, 4),
            model_used='ensemble(isolation_forest+one_class_svm)',
            features_used=self._get_feature_names(detection_type),
            explanation=self._generate_explanation(detection_type, raw_data, anomaly_type)
        )
    
    def _rule_based_detection(self, detection_type: str, 
                               data: Dict[str, Any]) -> Tuple[float, Optional[str]]:
        """基于规则的检测"""
        score = 0.0
        rule_type = None
        
        if detection_type == 'key':
            # 检查已暂停/撤销的钥匙
            if data.get('is_suspended') or data.get('is_revoked'):
                score = 1.0
                rule_type = 'suspended_key_used'
            # 检查超长距离
            elif data.get('key_distance_from_vehicle', 0) > 1000:
                score = 0.9
                rule_type = 'distance_anomaly'
            # 检查异常时间 (凌晨使用)
            elif data.get('usage_time_of_day', 12) in [0, 1, 2, 3, 4, 5]:
                score = 0.7
                rule_type = 'time_pattern_anomaly'
                
        elif detection_type == 'vehicle':
            # 检查未授权启动
            if data.get('unauthorized_key_id'):
                score = 1.0
                rule_type = 'unauthorized_start'
            # 检查大幅位置偏差
            elif data.get('location_deviation_meters', 0) > 10000:
                score = 0.9
                rule_type = 'location_anomaly'
                
        elif detection_type == 'user':
            # 检查不可能旅行
            velocity = data.get('calculated_velocity_kmh', 0)
            max_velocity = data.get('max_possible_velocity_kmh', 900)
            if velocity > max_velocity:
                score = 1.0
                rule_type = 'impossible_travel'
            # 检查暴力破解
            elif data.get('failed_auth_attempts', 0) > 5:
                score = 0.9
                rule_type = 'brute_force_attempt'
        
        return score, rule_type
    
    def _classify_anomaly(self, detection_type: str, data: Dict[str, Any], 
                          confidence: float, rule_type: Optional[str]) -> Tuple[str, AnomalySeverity]:
        """分类异常类型和严重程度"""
        
        severity = AnomalySeverity.LOW
        if confidence > 0.9:
            severity = AnomalySeverity.CRITICAL
        elif confidence > 0.75:
            severity = AnomalySeverity.HIGH
        elif confidence > 0.5:
            severity = AnomalySeverity.MEDIUM
        
        if rule_type:
            return rule_type, severity
        
        # 默认分类
        if detection_type == 'key':
            return 'unknown_key_anomaly', severity
        elif detection_type == 'vehicle':
            return 'unknown_vehicle_anomaly', severity
        else:
            return 'unknown_user_anomaly', severity
    
    def _get_feature_names(self, detection_type: str) -> List[str]:
        """获取特征名称"""
        if detection_type == 'key':
            return ['hour', 'hour_sin', 'hour_cos', 'distance', 'signal_strength',
                    'usage_count', 'is_authorized', 'is_suspended', 'is_revoked']
        elif detection_type == 'vehicle':
            return ['engine_running', 'door_anomaly', 'window_anomaly', 
                    'location_deviation', 'unauthorized_access', 'accel_magnitude', 
                    'ignition_legal']
        else:
            return ['access_count', 'unique_devices', 'unique_locations',
                    'failed_attempts', 'jump_distance', 'time_diff', 
                    'velocity_ratio', 'impossible_travel']
    
    def _generate_explanation(self, detection_type: str, data: Dict[str, Any], 
                               anomaly_type: str) -> str:
        """生成异常解释"""
        explanations = {
            'suspended_key_used': '检测到已暂停或撤销的钥匙被使用',
            'distance_anomaly': f"钥匙与车辆距离异常 ({data.get('key_distance_from_vehicle', 0):.0f}米)",
            'time_pattern_anomaly': f"异常时间使用 ({data.get('usage_time_of_day', 0)}:00)",
            'unauthorized_start': '检测到未授权的引擎启动',
            'location_anomaly': f"车辆位置偏离预期位置 {data.get('location_deviation_meters', 0):.0f}米",
            'impossible_travel': f"检测到不可能旅行 (速度: {data.get('calculated_velocity_kmh', 0):.0f}km/h)",
            'brute_force_attempt': f"检测到暴力破解尝试 ({data.get('failed_auth_attempts', 0)}次失败)",
        }
        return explanations.get(anomaly_type, f"检测到{detection_type}类型异常")
    
    async def detect_async(self, detection_type: str, 
                           data: Dict[str, Any]) -> DetectionResult:
        """异步检测"""
        loop = asyncio.get_event_loop()
        
        if detection_type == 'key':
            return await loop.run_in_executor(
                self.executor, self.detect_key_anomaly, data
            )
        elif detection_type == 'vehicle':
            return await loop.run_in_executor(
                self.executor, self.detect_vehicle_anomaly, data
            )
        else:
            return await loop.run_in_executor(
                self.executor, self.detect_user_behavior_anomaly, data
            )
    
    def batch_detect(self, detection_type: str, 
                     data_list: List[Dict[str, Any]]) -> List[DetectionResult]:
        """批量检测"""
        results = []
        for data in data_list:
            if detection_type == 'key':
                result = self.detect_key_anomaly(data)
            elif detection_type == 'vehicle':
                result = self.detect_vehicle_anomaly(data)
            else:
                result = self.detect_user_behavior_anomaly(data)
            results.append(result)
        return results
    
    def save_models(self, save_path: Optional[str] = None):
        """保存模型"""
        path = save_path or self.config.get('model_save_path', './models')
        os.makedirs(path, exist_ok=True)
        
        for detection_type in ['key', 'vehicle', 'user']:
            for model_name, model in self.models[detection_type].items():
                filename = f"{detection_type}_{model_name}.joblib"
                joblib.dump(model, os.path.join(path, filename))
            
            # 保存scaler
            scaler_filename = f"{detection_type}_scaler.joblib"
            joblib.dump(self.scalers[detection_type], os.path.join(path, scaler_filename))
        
        return {'save_path': path, 'models_saved': 6}
    
    def load_models(self, load_path: Optional[str] = None):
        """加载模型"""
        path = load_path or self.config.get('model_save_path', './models')
        
        if not os.path.exists(path):
            return {'loaded': False, 'error': 'Model path does not exist'}
        
        loaded_count = 0
        for detection_type in ['key', 'vehicle', 'user']:
            for model_name in ['isolation_forest', 'one_class_svm']:
                filename = f"{detection_type}_{model_name}.joblib"
                filepath = os.path.join(path, filename)
                if os.path.exists(filepath):
                    self.models[detection_type][model_name] = joblib.load(filepath)
                    loaded_count += 1
            
            # 加载scaler
            scaler_filename = f"{detection_type}_scaler.joblib"
            scaler_filepath = os.path.join(path, scaler_filename)
            if os.path.exists(scaler_filepath):
                self.scalers[detection_type] = joblib.load(scaler_filepath)
        
        return {'loaded': True, 'models_loaded': loaded_count}
    
    def get_stats(self) -> Dict[str, Any]:
        """获取检测统计信息"""
        return {
            'total_detections': self.detection_stats['total_detections'],
            'anomalies_detected': self.detection_stats['anomalies_detected'],
            'detection_rate': (
                self.detection_stats['anomalies_detected'] / 
                max(self.detection_stats['total_detections'], 1)
            ),
            'buffer_sizes': {
                k: len(v) for k, v in self.data_buffer.items()
            }
        }


# 全局检测器实例
detector = AnomalyDetector()


def get_detector() -> AnomalyDetector:
    """获取全局检测器实例"""
    return detector
