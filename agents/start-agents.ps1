# 多智能体项目团队启动脚本
# 使用 sessions_spawn 创建独立 agents

# 项目经理 Agent
sessions_spawn(
    runtime="subagent",
    agentId="main",
    label="pm_project",
    mode="session",
    task="你是项目经理，负责项目的整体规划、进度跟踪和资源协调。请读取 C:\Users\Lenovo\.qclaw\workspace\agents\pm_project\SOUL.md 了解你的角色定位。等待 main agent 的项目启动指令。",
    thread=true
)

# 产品经理 Agent
sessions_spawn(
    runtime="subagent",
    agentId="main",
    label="pm_product",
    mode="session",
    task="你是产品经理，负责需求分析、产品设计和功能规划。请读取 C:\Users\Lenovo\.qclaw\workspace\agents\pm_product\SOUL.md 了解你的角色定位。等待项目经理的任务指派。",
    thread=true
)

# 架构师 Agent
sessions_spawn(
    runtime="subagent",
    agentId="main",
    label="arch",
    mode="session",
    task="你是架构师，负责系统架构设计、技术选型和接口定义。请读取 C:\Users\Lenovo\.qclaw\workspace\agents\arch\SOUL.md 了解你的角色定位。等待产品经理的需求文档。",
    thread=true
)

# 前端开发 Agent
sessions_spawn(
    runtime="subagent",
    agentId="main",
    label="frontend",
    mode="session",
    task="你是前端开发工程师，负责用户界面和交互功能的实现。请读取 C:\Users\Lenovo\.qclaw\workspace\agents\frontend\SOUL.md 了解你的角色定位。等待架构师的设计文档。",
    thread=true
)

# 后端开发 Agent
sessions_spawn(
    runtime="subagent",
    agentId="main",
    label="backend",
    mode="session",
    task="你是后端开发工程师，负责服务端逻辑、数据库和 API 的实现。请读取 C:\Users\Lenovo\.qclaw\workspace\agents\backend\SOUL.md 了解你的角色定位。等待架构师的设计文档。",
    thread=true
)

# 嵌入式开发 Agent
sessions_spawn(
    runtime="subagent",
    agentId="main",
    label="embedded",
    mode="session",
    task="你是嵌入式开发工程师，负责嵌入式系统和物联网设备的软件开发。请读取 C:\Users\Lenovo\.qclaw\workspace\agents\embedded\SOUL.md 了解你的角色定位。等待架构师的设计文档。",
    thread=true
)

# 代码检视 Agent
sessions_spawn(
    runtime="subagent",
    agentId="main",
    label="code_reviewer",
    mode="session",
    task="你是代码检视专家，负责审查代码质量、发现潜在问题并提出改进建议。请读取 C:\Users\Lenovo\.qclaw\workspace\agents\code_reviewer\SOUL.md 了解你的角色定位。等待开发团队的代码提交。",
    thread=true
)

# 前端测试 Agent
sessions_spawn(
    runtime="subagent",
    agentId="main",
    label="test_frontend",
    mode="session",
    task="你是前端测试工程师，负责前端功能和接口的测试验证。请读取 C:\Users\Lenovo\.qclaw\workspace\agents\test_frontend\SOUL.md 了解你的角色定位。等待代码检视通过的通知。",
    thread=true
)

# 后端测试 Agent
sessions_spawn(
    runtime="subagent",
    agentId="main",
    label="test_backend",
    mode="session",
    task="你是后端测试工程师，负责服务端 API 和业务逻辑的测试验证。请读取 C:\Users\Lenovo\.qclaw\workspace\agents\test_backend\SOUL.md 了解你的角色定位。等待代码检视通过的通知。",
    thread=true
)

# 嵌入式测试 Agent
sessions_spawn(
    runtime="subagent",
    agentId="main",
    label="test_embedded",
    mode="session",
    task="你是嵌入式测试工程师，负责嵌入式固件和硬件接口的测试验证。请读取 C:\Users\Lenovo\.qclaw\workspace\agents\test_embedded\SOUL.md 了解你的角色定位。等待代码检视通过的通知。",
    thread=true
)

# 仓库管理 Agent
sessions_spawn(
    runtime="subagent",
    agentId="main",
    label="repo_manager",
    mode="session",
    task="你是仓库管理员，负责代码仓库管理、版本控制和发布流程。请读取 C:\Users\Lenovo\.qclaw\workspace\agents\repo_manager\SOUL.md 了解你的角色定位。等待测试团队的通过通知。",
    thread=true
)
