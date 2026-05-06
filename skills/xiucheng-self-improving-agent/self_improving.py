#!/usr/bin/env python3
"""Self-Improving Agent - Continuous improvement system for OpenClaw agents.

Author: xiucheng
Version: 1.0.0
License: MIT
"""

import os
import json
import re
from datetime import datetime, timedelta
from pathlib import Path
from typing import List, Dict, Optional, Any


class SelfImprovingAgent:
    """Analyzes and improves agent performance over time."""
    
    def __init__(self, workspace: str = "/root/.openclaw/workspace"):
        """Initialize self-improving agent.
        
        Args:
            workspace: Path to OpenClaw workspace
        """
        self.workspace = Path(workspace)
        self.improvement_log = self.workspace / "improvement_log.md"
        self.soul_file = self.workspace / "SOUL.md"
        
    def analyze_conversation(
        self, 
        conversation: str, 
        feedback: Optional[str] = None,
        metrics: Optional[Dict] = None
    ) -> Dict[str, Any]:
        """Analyze conversation quality.
        
        Args:
            conversation: Conversation text to analyze
            feedback: Optional user feedback
            metrics: Optional performance metrics
            
        Returns:
            Analysis results dict
        """
        analysis = {
            "timestamp": datetime.now().isoformat(),
            "length": len(conversation),
            "feedback": feedback,
            "improvements": [],
            "strengths": []
        }
        
        # Heuristic analysis
        if feedback:
            if any(word in feedback for word in ["好", "棒", "优秀", "good", "great"]):
                analysis["strengths"].append("Current approach is effective")
            if any(word in feedback for word in ["慢", "慢", "slow", "delay"]):
                analysis["improvements"].append("Improve response speed")
            if any(word in feedback for word in ["长", "啰嗦", "verbose", "long"]):
                analysis["improvements"].append("Be more concise")
        
        # Length-based analysis
        if len(conversation) > 5000:
            analysis["improvements"].append("Consider summarizing long responses")
        elif len(conversation) < 100:
            analysis["improvements"].append("Provide more detailed explanations")
        else:
            analysis["strengths"].append("Response length is appropriate")
        
        return analysis
    
    def log_improvement(self, insight: str, category: str = "general") -> None:
        """Log an improvement insight.
        
        Args:
            insight: The improvement insight
            category: Category (e.g., 'communication', 'technical', 'speed')
        """
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M")
        
        with open(self.improvement_log, "a", encoding="utf-8") as f:
            f.write(f"\n## [{timestamp}] {category}\n")
            f.write(f"- {insight}\n")
    
    def generate_weekly_report(self) -> str:
        """Generate weekly improvement report.
        
        Returns:
            Formatted report string
        """
        week_start = datetime.now() - timedelta(days=7)
        
        report_lines = [
            "# 🔄 Self-Improvement Weekly Report",
            f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M')}",
            "",
            "## 📊 This Week's Insights",
        ]
        
        # Read improvement log
        if self.improvement_log.exists():
            with open(self.improvement_log, "r", encoding="utf-8") as f:
                content = f.read()
                # Count entries from this week
                entries = [line for line in content.split("\n## ") 
                          if week_start.strftime("%Y-%m-%d") in line]
                report_lines.append(f"- Total improvements logged: {len(entries)}")
        else:
            report_lines.append("- No improvements logged yet")
        
        report_lines.extend([
            "",
            "## 🎯 Next Week Goals",
            "- Continue monitoring conversation quality",
            "- Identify patterns in user feedback",
            "- Update response strategies based on insights",
            "",
            "---",
            "*Keep improving, one conversation at a time!*"
        ])
        
        return "\n".join(report_lines)
    
    def get_improvement_stats(self) -> Dict[str, Any]:
        """Get improvement statistics.
        
        Returns:
            Statistics dict
        """
        stats = {
            "log_exists": self.improvement_log.exists(),
            "soul_exists": self.soul_file.exists(),
            "total_entries": 0
        }
        
        if self.improvement_log.exists():
            with open(self.improvement_log, "r", encoding="utf-8") as f:
                content = f.read()
                stats["total_entries"] = content.count("\n## [")
                stats["log_size_kb"] = round(len(content) / 1024, 2)
        
        return stats
    
    def suggest_soul_updates(self) -> List[str]:
        """Suggest updates to SOUL.md based on learnings.
        
        Returns:
            List of suggested updates
        """
        suggestions = []
        
        if not self.improvement_log.exists():
            return ["Start logging improvements to generate suggestions"]
        
        with open(self.improvement_log, "r", encoding="utf-8") as f:
            content = f.read()
            
        # Analyze patterns
        if "concise" in content.lower() or "简洁" in content:
            suggestions.append("Consider adding 'conciseness' to personality traits")
        if "speed" in content.lower() or "慢" in content:
            suggestions.append("Consider emphasizing efficiency in work style")
        
        return suggestions if suggestions else ["No specific updates suggested at this time"]


def main():
    """CLI entry point."""
    import argparse
    
    parser = argparse.ArgumentParser(description="Self-Improving Agent CLI")
    parser.add_argument("--log", "-l", help="Log an improvement insight")
    parser.add_argument("--category", "-c", default="general", help="Category")
    parser.add_argument("--report", "-r", action="store_true", help="Generate weekly report")
    parser.add_argument("--stats", "-s", action="store_true", help="Show stats")
    parser.add_argument("--suggest", action="store_true", help="Suggest SOUL updates")
    
    args = parser.parse_args()
    
    sia = SelfImprovingAgent()
    
    if args.log:
        sia.log_improvement(args.log, args.category)
        print(f"✅ Logged: {args.log}")
    elif args.report:
        print(sia.generate_weekly_report())
    elif args.stats:
        print(json.dumps(sia.get_improvement_stats(), indent=2))
    elif args.suggest:
        suggestions = sia.suggest_soul_updates()
        print("Suggested SOUL.md updates:")
        for s in suggestions:
            print(f"  - {s}")
    else:
        print("🔄 Self-Improving Agent initialized!")
        print(f"Stats: {sia.get_improvement_stats()}")


if __name__ == "__main__":
    main()
