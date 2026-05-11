"""alerting.py - AI 异常检测预警通知系统

支持通知渠道:
- Email (邮件)
- SMS (短信)
- Webhook (企业微信/钉钉/飞书)
- Push Notification (推送通知)
- MQTT (实时消息)
"""

import json
import smtplib
import requests
from datetime import datetime
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from typing import List, Dict, Optional, Callable
from dataclasses import dataclass
from enum import Enum
import logging

# 配置日志
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class AlertSeverity(Enum):
    """预警严重级别"""
    CRITICAL = "critical"    # 严重 - 立即处理
    HIGH = "high"            # 高 - 1小时内处理
    MEDIUM = "medium"        # 中 - 24小时内处理
    LOW = "low"              # 低 - 记录即可


class AlertChannel(Enum):
    """通知渠道"""
    EMAIL = "email"
    SMS = "sms"
    WEBHOOK = "webhook"
    PUSH = "push"
    MQTT = "mqtt"


@dataclass
class AlertConfig:
    """预警配置"""
    # Email 配置
    smtp_host: str = "smtp.example.com"
    smtp_port: int = 587
    smtp_user: str = ""
    smtp_password: str = ""
    email_from: str = "alerts@yuledkcs.com"
    email_to: List[str] = None
    
    # SMS 配置 (使用云服务商接口)
    sms_provider: str = "aliyun"  # aliyun, tencent, twilio
    sms_access_key: str = ""
    sms_secret_key: str = ""
    sms_sign_name: str = ""
    sms_template_code: str = ""
    sms_phone_numbers: List[str] = None
    
    # Webhook 配置
    webhook_url: str = ""
    webhook_secret: str = ""
    
    # Push 配置 (Firebase/个推/友盟)
    push_provider: str = ""
    push_api_key: str = ""
    
    # MQTT 配置
    mqtt_broker: str = "localhost"
    mqtt_port: int = 1883
    mqtt_topic: str = "yuledkcs/alerts"
    
    def __post_init__(self):
        if self.email_to is None:
            self.email_to = []
        if self.sms_phone_numbers is None:
            self.sms_phone_numbers = []


@dataclass
class Alert:
    """预警信息"""
    id: str
    severity: AlertSeverity
    anomaly_type: str  # key, vehicle, user_behavior
    title: str
    description: str
    details: Dict
    timestamp: datetime
    vehicle_id: Optional[str] = None
    user_id: Optional[str] = None
    key_id: Optional[str] = None
    location: Optional[Dict] = None
    
    def to_dict(self) -> Dict:
        return {
            "id": self.id,
            "severity": self.severity.value,
            "anomaly_type": self.anomaly_type,
            "title": self.title,
            "description": self.description,
            "details": self.details,
            "timestamp": self.timestamp.isoformat(),
            "vehicle_id": self.vehicle_id,
            "user_id": self.user_id,
            "key_id": self.key_id,
            "location": self.location
        }


class AlertManager:
    """预警管理器 - 统一管理所有通知渠道"""
    
    def __init__(self, config: AlertConfig):
        self.config = config
        self.handlers: Dict[AlertChannel, Callable] = {
            AlertChannel.EMAIL: self._send_email,
            AlertChannel.SMS: self._send_sms,
            AlertChannel.WEBHOOK: self._send_webhook,
            AlertChannel.PUSH: self._send_push,
            AlertChannel.MQTT: self._send_mqtt,
        }
        self.alert_history: List[Alert] = []
        
    def send_alert(self, alert: Alert, channels: List[AlertChannel] = None):
        """
        发送预警
        
        Args:
            alert: 预警信息
            channels: 指定通知渠道，默认根据严重级自动选择
        """
        if channels is None:
            channels = self._get_default_channels(alert.severity)
        
        # 记录预警
        self.alert_history.append(alert)
        
        # 并行发送到所有指定渠道
        for channel in channels:
            try:
                handler = self.handlers.get(channel)
                if handler:
                    handler(alert)
                    logger.info(f"预警 [{alert.id}] 已通过 {channel.value} 发送")
            except Exception as e:
                logger.error(f"通过 {channel.value} 发送预警失败: {e}")
    
    def _get_default_channels(self, severity: AlertSeverity) -> List[AlertChannel]:
        """根据严重级别获取默认通知渠道"""
        channels = [AlertChannel.WEBHOOK]  # 所有级别都发送Webhook
        
        if severity == AlertSeverity.CRITICAL:
            channels.extend([
                AlertChannel.EMAIL,
                AlertChannel.SMS,
                AlertChannel.PUSH,
                AlertChannel.MQTT
            ])
        elif severity == AlertSeverity.HIGH:
            channels.extend([
                AlertChannel.EMAIL,
                AlertChannel.PUSH,
                AlertChannel.MQTT
            ])
        elif severity == AlertSeverity.MEDIUM:
            channels.extend([
                AlertChannel.EMAIL,
                AlertChannel.MQTT
            ])
        
        return channels
    
    def _send_email(self, alert: Alert):
        """发送邮件预警"""
        if not self.config.email_to:
            return
        
        msg = MIMEMultipart()
        msg['From'] = self.config.email_from
        msg['To'] = ', '.join(self.config.email_to)
        msg['Subject'] = f"[yuleDKCS] [{alert.severity.value.upper()}] {alert.title}"
        
        # 邮件内容
        body = f"""
<h2>🚨 异常检测预警</h2>

<p><strong>严重级别:</strong> {alert.severity.value.upper()}</p>
<p><strong>类型:</strong> {alert.anomaly_type}</p>
<p><strong>时间:</strong> {alert.timestamp}</p>
<p><strong>描述:</strong> {alert.description}</p>

<h3>详细信息</h3>
<pre>{json.dumps(alert.details, indent=2, ensure_ascii=False)}</pre>

<hr>
<p><small>此由 yuleDKCS AI 异常检测系统自动发送</small></p>
"""
        
        msg.attach(MIMEText(body, 'html', 'utf-8'))
        
        try:
            with smtplib.SMTP(self.config.smtp_host, self.config.smtp_port) as server:
                server.starttls()
                server.login(self.config.smtp_user, self.config.smtp_password)
                server.send_message(msg)
        except Exception as e:
            logger.error(f"Email 发送失败: {e}")
    
    def _send_sms(self, alert: Alert):
        """发送短信预警 (示例: 阿里云短信)"""
        if not self.config.sms_phone_numbers:
            return
        
        if self.config.sms_provider == "aliyun":
            self._send_aliyun_sms(alert)
        else:
            logger.warning(f"未支持的短信服务商: {self.config.sms_provider}")
    
    def _send_aliyun_sms(self, alert: Alert):
        """发送阿里云短信"""
        # 这里只是示例，实际使用需要导入 aliyunsdk
        url = "https://dysmsapi.aliyuncs.com"
        
        for phone in self.config.sms_phone_numbers:
            params = {
                "PhoneNumbers": phone,
                "SignName": self.config.sms_sign_name,
                "TemplateCode": self.config.sms_template_code,
                "TemplateParam": json.dumps({
                    "severity": alert.severity.value,
                    "type": alert.anomaly_type,
                    "time": alert.timestamp.strftime("%H:%M")
                })
            }
            logger.info(f"模拟发送阿里云短信到 {phone}: {params}")
    
    def _send_webhook(self, alert: Alert):
        """发送 Webhook (企业微信/钉钉/飞书)"""
        if not self.config.webhook_url:
            return
        
        # 企业微信格式
        wechat_msg = {
            "msgtype": "markdown",
            "markdown": {
                "content": f"""🚨 **yuleDKCS 异常检测预警**
                
>严重级别: <font color='{"red" if alert.severity == AlertSeverity.CRITICAL else "warning"}'>{alert.severity.value.upper()}</font>
>类型: {alert.anomaly_type}
>时间: {alert.timestamp}
>描述: {alert.description}

**详细信息:**
```json
{json.dumps(alert.details, indent=2, ensure_ascii=False)}
```
"""
            }
        }
        
        try:
            response = requests.post(
                self.config.webhook_url,
                json=wechat_msg,
                headers={"Content-Type": "application/json"},
                timeout=10
            )
            response.raise_for_status()
        except Exception as e:
            logger.error(f"Webhook 发送失败: {e}")
    
    def _send_push(self, alert: Alert):
        """发送推送通知"""
        # 示例: Firebase Cloud Messaging
        if not self.config.push_api_key:
            return
        
        push_msg = {
            "to": "/topics/alerts",
            "notification": {
                "title": f"🚨 {alert.severity.value.upper()}: {alert.title}",
                "body": alert.description
            },
            "data": alert.to_dict()
        }
        
        logger.info(f"模拟发送推送通知: {push_msg}")
    
    def _send_mqtt(self, alert: Alert):
        """发送 MQTT 消息"""
        try:
            import paho.mqtt.publish as publish
            
            publish.single(
                self.config.mqtt_topic,
                json.dumps(alert.to_dict()),
                hostname=self.config.mqtt_broker,
                port=self.config.mqtt_port
            )
        except ImportError:
            logger.warning("paho-mqtt 未安装，跳过 MQTT 发送")
        except Exception as e:
            logger.error(f"MQTT 发送失败: {e}")
    
    def create_alert(
        self,
        severity: AlertSeverity,
        anomaly_type: str,
        title: str,
        description: str,
        details: Dict,
        **kwargs
    ) -> Alert:
        """便捷方法: 创建并发送预警"""
        alert = Alert(
            id=f"ALT-{datetime.now().strftime('%Y%m%d%H%M%S')}-{id(self)}",
            severity=severity,
            anomaly_type=anomaly_type,
            title=title,
            description=description,
            details=details,
            timestamp=datetime.now(),
            **kwargs
        )
        
        self.send_alert(alert)
        return alert


# 使用示例
if __name__ == "__main__":
    # 配置
    config = AlertConfig(
        email_to=["security@example.com"],
        webhook_url="https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=xxx"
    )
    
    # 创建预警管理器
    alert_manager = AlertManager(config)
    
    # 创建异常检测预警
    alert_manager.create_alert(
        severity=AlertSeverity.CRITICAL,
        anomaly_type="key",
        title="检测到非法钥匙使用",
        description="发现一个已被注销的钥匙尝试解锁车辆",
        details={
            "key_id": "key-12345",
            "vehicle_id": "VIN-LSVAG2180E2100001",
            "location": {"lat": 39.9042, "lon": 116.4074},
            "attempt_time": "2026-05-09T15:30:00Z",
            "confidence": 0.95
        }
    )
    
    print("预警发送完成!")
