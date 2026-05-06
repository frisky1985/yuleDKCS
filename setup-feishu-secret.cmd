@echo off
chcp 65001 >nul
echo ==========================================
echo    飞书密钥配置脚本
echo ==========================================
echo.

:: 检查 App ID
if "%FEISHU_APP_ID%"=="" (
    echo 请输入你的飞书 App ID (cli_xxxxxx 格式):
    set /p FEISHU_APP_ID=
)

if "%FEISHU_APP_ID%"=="" (
    echo [错误] App ID 不能为空
    pause
    exit /b 1
)

echo.
echo 请输入你的飞书 App Secret:
set /p FEISHU_APP_SECRET=

if "%FEISHU_APP_SECRET%"=="" (
    echo [错误] App Secret 不能为空
    pause
    exit /b 1
)

:: 创建密钥目录
if not exist "%USERPROFILE%\.clawdbot\secrets" (
    mkdir "%USERPROFILE%\.clawdbot\secrets"
)

:: 保存密钥
echo %FEISHU_APP_SECRET% > "%USERPROFILE%\.clawdbot\secrets\feishu_app_secret"

echo.
echo [成功] 密钥已保存到: %USERPROFILE%\.clawdbot\secrets\feishu_app_secret

:: 创建环境变量配置文件（可选，方便后续使用）
echo FEISHU_APP_ID=%FEISHU_APP_ID% > "%USERPROFILE%\.qclaw\feishu-env.bat"

echo.
echo ==========================================
echo    配置完成！
echo ==========================================
echo.
echo 接下来请：
echo 1. 将 FEISHU_APP_ID 添加到系统环境变量
echo    - 在 Windows 搜索框输入 "环境变量"
echo    - 点击 "编辑系统环境变量"
echo    - 点击 "环境变量" 按钮
echo    - 在用户变量中点击 "新建"
echo    - 变量名: FEISHU_APP_ID
echo    - 变量值: %FEISHU_APP_ID%
echo.
echo 2. 运行 start-feishu-bridge.cmd 启动飞书 Bridge
echo.
pause
