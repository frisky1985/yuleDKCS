# 数字钥匙协议学习总结

**项目**: pm_project - 数字钥匙产品开发  
**学习日期**: 2026-05-05  
**来源**: IMA知识库《数字钥匙》+ WebFetch

---

## 一、CCC (Car Connectivity Consortium)

### 发起成员
Apple, BMW, Ford, GM, Google, Honda, Hyundai, Mercedes-Benz, Samsung等

### 规范版本
- CCC 3.0 (Digital Key Release 3.0)
- CCC 4.0 / 4.1.0 (当前最新)

### 核心技术架构

#### 4.1 Applet Instance Layout
数字密钥小程序实例管理所有数据：
- 多个数字密钥 (Digital Keys)
- 一个或多个 Instance CAs（设备OEM CA中间证书）
- **每个车厂有唯一的Instance CA**

#### 4.2 Digital Key Structure

| 数据单元 | 说明 |
|---------|------|
| Vehicle Identifier | 8字节车辆标识符，唯一标识车辆 |
| Endpoint Identifier | 设备内部密钥管理用 |
| Digital Key Identifier (keyID) | 公钥SHA-1哈希值，160bit |
| Slot Identifier | 密钥标识符，车辆提供 |
| Instance CA Identifier | 指向签名的Instance CA |
| Key Options | 密钥选项（快速/标准交易等） |
| Device Public Key | 设备公钥 |
| Vehicle Public Key | 车辆公钥（同车所有设备相同）|
| Authorized public keys | 车辆提供的授权公钥（5个）|
| Private Mailbox | NFC交易数据缓存 |
| Confidential Mailbox | 加密保护的数据缓存 |

#### Attestation Package（权限证明包）
- Friend Public Key：朋友设备公钥
- Profile：访问权限配置
- Sharing password information：分享密码信息
- Validity start/end date：有效期
- Key friendly name：用户友好名称

### 通信技术
- **NFC**：低功耗近场通信，触碰配对
- **BLE (Bluetooth Low Energy)**：广播配对、密钥传输
- **UWB (Ultra-Wideband)**：精准测距，安全测距

---

## 二、ICCOA (智慧车联开放联盟)

### 发起单位
小米、OPPO、vivo三家手机厂商 + 国内主流车企

### 使命
- 移动终端和汽车之间的互联互通
- 减少产业碎片化
- 促进手机与汽车的深度融合

### 规范版本
- ICCOA DK 3.0
- ICCOA DK 4.0

### 知识库文档
- ICCOA DK 3.0.pdf
- ICCOA DK 4.0_251201.pdf

---

## 三、ICCE (Automotive Edge Computing Consortium)

### 定位
车端边缘计算联盟

### 知识库文档
- ICCE 智慧车联产业生态联盟数字车钥匙系统 第2部分：蓝牙系统规范.pdf

---

## 四、三大协议对比

| 协议 | 发起方 | 手机厂商 | 汽车厂商 | 特点 |
|-----|--------|---------|---------|------|
| CCC | 国际厂商 | Apple/三星 | 美德日韩 | 全球标准，生态最广 |
| ICCOA | 中国厂商 | 米/OV | 国内车企 | 本土化，生态整合 |
| ICCE | 车厂联盟 | - | 国内外 | 边缘计算，车云一体 |

---

## 五、学习资源汇总

### 知识库中的关键资料
1. CCC规范：CCC-TS-101-Digital-Key v3.1.4/4.0.0/4.1.0
2. CCC白皮书：CCC_Digital_Key_Whitepaper_3.0
3. 学习笔记：CSDN博客多篇
4. ICCOA规范：ICCOA DK 3.0/4.0
5. ICCE规范：蓝牙系统规范

### 关键URL（需IMA客户端查看）
- PDF文件：需下载查看
- URL链接：可在线获取