import { useEffect, useState } from 'react'
import {
  Box,
  Grid,
  Paper,
  Typography,
  Card,
  CardContent,
  List,
  ListItem,
  ListItemText,
  Divider,
} from '@mui/material'
import {
  AccountBalance as AccountIcon,
  ShoppingCart as OrderIcon,
  People as UserIcon,
  TrendingUp as TrendIcon,
} from '@mui/icons-material'

interface DashboardStats {
  totalUsers: number
  totalOrders: number
  totalRevenue: number
  todayOrders: number
}

const DashboardPage = () => {
  const [stats, setStats] = useState<DashboardStats>({
    totalUsers: 0,
    totalOrders: 0,
    totalRevenue: 0,
    todayOrders: 0,
  })

  useEffect(() => {
    // TODO: 从 API 获取统计数据
    // 这里使用模拟数据
    setStats({
      totalUsers: 150,
      totalOrders: 1250,
      totalRevenue: 45680.5,
      todayOrders: 23,
    })
  }, [])

  const statCards = [
    { title: '总用户数', value: stats.totalUsers, icon: <UserIcon /> },
    { title: '总订单数', value: stats.totalOrders, icon: <OrderIcon /> },
    { title: '总收入', value: `¥${stats.totalRevenue.toFixed(2)}`, icon: <AccountIcon /> },
    { title: '今日订单', value: stats.todayOrders, icon: <TrendIcon /> },
  ]

  return (
    <Box>
      <Typography variant="h4" gutterBottom>
        仪表板
      </Typography>

      <Grid container spacing={3} sx={{ mb: 4 }}>
        {statCards.map((card, index) => (
          <Grid item xs={12} sm={6} md={3} key={index}>
            <Card>
              <CardContent>
                <Box sx={{ display: 'flex', alignItems: 'center', mb: 1 }}>
                  <Box sx={{ color: 'primary.main', mr: 1 }}>{card.icon}</Box>
                  <Typography color="textSecondary" variant="body2">
                    {card.title}
                  </Typography>
                </Box>
                <Typography variant="h5" component="div">
                  {card.value}
                </Typography>
              </CardContent>
            </Card>
          </Grid>
        ))}
      </Grid>

      <Grid container spacing={3}>
        <Grid item xs={12} md={6}>
          <Paper sx={{ p: 2 }}>
            <Typography variant="h6" gutterBottom>
              最近订单
            </Typography>
            <List>
              <ListItem>
                <ListItemText
                  primary="订单 #12345"
                  secondary="2024-01-15 14:30 - ¥100.00"
                />
              </ListItem>
              <Divider />
              <ListItem>
                <ListItemText
                  primary="订单 #12344"
                  secondary="2024-01-15 13:20 - ¥50.00"
                />
              </ListItem>
              <Divider />
              <ListItem>
                <ListItemText
                  primary="订单 #12343"
                  secondary="2024-01-15 11:45 - ¥200.00"
                />
              </ListItem>
            </List>
          </Paper>
        </Grid>

        <Grid item xs={12} md={6}>
          <Paper sx={{ p: 2 }}>
            <Typography variant="h6" gutterBottom>
              系统公告
            </Typography>
            <List>
              <ListItem>
                <ListItemText
                  primary="系统维护通知"
                  secondary="2024-01-20 00:00 - 04:00 进行系统维护"
                />
              </ListItem>
              <Divider />
              <ListItem>
                <ListItemText
                  primary="新功能上线"
                  secondary="批量充值功能已正式上线"
                />
              </ListItem>
              <Divider />
              <ListItem>
                <ListItemText
                  primary="充值优惠活动"
                  secondary="春节期间充值享 95 折优惠"
                />
              </ListItem>
            </List>
          </Paper>
        </Grid>
      </Grid>
    </Box>
  )
}

export default DashboardPage
