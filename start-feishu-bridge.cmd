@echo off
chcp 65001 >nul
echo ==========================================
echo    OpenClaw 飞书 Bridge 启动脚本
echo ==========================================
echo.

:: 检查环境变量
if "%FEISHU_APP_ID%"=="" (
    echo [错误] 未设置 FEISHU_APP_ID 环境变量
    echo 请先设置环境变量：set FEISHU_APP_ID=cli_xxx
    echo.
    pause
    exit /b 1
)

:: 检查密钥文件
set SECRET_PATH=%USERPROFILE%\.clawdbot\secrets\feishu_app_secret
if not exist "%SECRET_PATH%" (
    echo [错误] 未找到密钥文件: %SECRET_PATH%
    echo 请先创建密钥文件
    echo.
    pause
    exit /b 1
)

:: 检查 QClaw 配置
set QCLAW_CONFIG=%USERPROFILE%\.qclaw\qclaw.json
if not exist "%QCLAW_CONFIG%" (
    echo [错误] 未找到 QClaw 配置文件: %QCLAW_CONFIG%
    echo 请确保 QClaw 已正确安装
    echo.
    pause
    exit /b 1
)

echo [信息] 环境检查通过
echo [信息] App ID: %FEISHU_APP_ID%
echo [信息] 密钥文件: %SECRET_PATH%
echo.

:: 进入 bridge 目录
cd /d "%~dp0\..\skills\feishu-bridge"

:: 检查 node_modules
if not exist "node_modules" (
    echo [信息] 首次运行，正在安装依赖...
    call npm install
    if errorlevel 1 (
        echo [错误] 依赖安装失败
        pause
        exit /b 1
    )
)

echo [信息] 正在启动飞书 Bridge...
echo [信息] 按 Ctrl+C 停止服务
echo.

:: 启动 bridge
node bridge.mjs

pause
