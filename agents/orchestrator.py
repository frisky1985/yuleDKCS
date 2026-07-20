# 项目总指挥控制器
# 用于 main agent 协调多智能体项目团队

class ProjectOrchestrator:
    """项目总指挥，协调整个多智能体团队"""
    
    AGENTS = {
        "pm_project": "agent:main:subagent:b3ee6551-33ae-4d39-9aa8-23b8015a6f2f",
        "pm_product": "agent:main:subagent:bf15b2b5-0c9e-4837-b428-fdb74b79f2b6",
        "arch": "agent:main:subagent:43bab61f-da58-4fc9-a65f-2e30ba3dcdf4",
        "frontend": "agent:main:subagent:23c68213-7e33-4534-8146-371f7aae6d89",
        "backend": "agent:main:subagent:e2722274-ccda-4356-8857-91b43cab195b",
        "embedded": "agent:main:subagent:d463c088-414e-415c-8fa1-50f24f995489",
        "code_reviewer": "agent:main:subagent:d15ca67e-a20b-454c-98ce-fe733db975bb",
        "test_frontend": "agent:main:subagent:f64d18a7-cdee-4cdc-9aef-586243ea5b52",
        "test_backend": "agent:main:subagent:8efdfc15-80ec-47d8-997b-f90a03a074d5",
        "test_embedded": "agent:main:subagent:4237d156-7013-45f4-8225-fb7e72201792",
        "repo_manager": "agent:main:subagent:e562e3ec-4339-41ef-a032-80efa6df32d4",
    }
    
    @staticmethod
    def start_project(project_name, description, deadline):
        """启动新项目"""
        message = f"""【项目启动】
项目名称：{project_name}
项目描述：{description}
截止日期：{deadline}

请作为项目经理：
1. 制定项目计划
2. 拆解任务清单
3. 协调各角色协作
4. 定期汇报进度

请开始工作。"""
        
        sessions_send(label="pm_project", message=message)
        return f"已通知项目经理启动项目：{project_name}"
    
    @staticmethod
    def assign_to_product_manager(tasks, deadline):
        """指派任务给产品经理"""
        message = f"""【需求设计任务】
任务内容：{tasks}
截止日期：{deadline}

请完成：
1. 需求分析和调研
2. PRD 文档编写
3. 用户故事设计
4. 功能清单拆解

完成后通知架构师进行技术设计。"""
        
        sessions_send(label="pm_product", message=message)
    
    @staticmethod
    def assign_to_architect(prd_content, requirements):
        """指派架构设计任务"""
        message = f"""【架构设计任务】
需求文档：{prd_content}
功能需求：{requirements}

请完成：
1. 系统架构设计
2. 技术选型
3. 接口规范定义
4. 数据库设计

完成后通知开发团队。"""
        
        sessions_send(label="arch", message=message)
    
    @staticmethod
    def assign_development(arch_design, frontend_tasks, backend_tasks, embedded_tasks):
        """指派开发任务"""
        # 前端开发
        sessions_send(label="frontend", message=f"""【前端开发任务】
架构设计：{arch_design}
开发内容：{frontend_tasks}

请完成前端功能开发，完成后提交代码检视。""")
        
        # 后端开发
        sessions_send(label="backend", message=f"""【后端开发任务】
架构设计：{arch_design}
开发内容：{backend_tasks}

请完成后端 API 开发，完成后提交代码检视。""")
        
        # 嵌入式开发
        sessions_send(label="embedded", message=f"""【嵌入式开发任务】
架构设计：{arch_design}
开发内容：{embedded_tasks}

请完成固件开发，完成后提交代码检视。""")
    
    @staticmethod
    def request_code_review(developer, code_content):
        """请求代码检视"""
        sessions_send(label="code_reviewer", message=f"""【代码检视请求】
来自：{developer}
代码内容：{code_content}

请进行代码审查，检查：
1. 代码规范和风格
2. 逻辑正确性
3. 安全漏洞
4. 性能问题
5. 最佳实践

请给出检视结果。""")
    
    @staticmethod
    def start_testing(component, test_requirements):
        """启动测试"""
        test_agent = f"test_{component}"
        sessions_send(label=test_agent, message=f"""【测试任务】
测试内容：{test_requirements}

请完成：
1. 测试用例设计
2. 功能测试执行
3. 问题记录和报告
4. 测试报告提交

请开始测试。""")
    
    @staticmethod
    def release_project(version, release_notes):
        """发布项目"""
        sessions_send(label="repo_manager", message=f"""【发布任务】
版本号：{version}
发布说明：{release_notes}

请完成：
1. 代码合并到主分支
2. 打版本标签
3. 生成发布说明
4. 通知项目经理

请执行发布流程。""")


# 使用示例：
# orchestrator = ProjectOrchestrator()
# orchestrator.start_project("电商平台", "构建支持Web和移动端的电商平台", "2024-06-01")
