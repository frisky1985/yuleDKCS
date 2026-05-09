import { useEffect, useState } from 'react';
import { 
  Box, Typography, Card, CardContent, Button, Grid, 
  Avatar, Chip, List, ListItem, ListItemText, ListItemAvatar,
  IconButton, LinearProgress, Fab, Badge
} from '@mui/material';
import { 
  DirectionsCar, LockOpen, Lock, TrackChanges, 
  Notifications, Settings, ChevronRight, Add
} from '@mui/icons-material';
import { motion } from 'framer-motion';
import { useQuery } from '@tanstack/react-query';
import { useNavigate } from 'react-router-dom';
import { api } from '../api/client';
import { useAuthStore } from '../store/auth';

interface Vehicle {
  id: number;
  brand: string;
  model: string;
  year: number;
  status: 'online' | 'offline' | 'locked' | 'unlocked';
  battery_level: number;
  location: { lat: number; lng: number };
}

interface Activity {
  id: number;
  type: string;
  message: string;
  timestamp: string;
  vehicle: string;
}

export default function DashboardPage() {
  const navigate = useNavigate();
  const { user } = useAuthStore();
  const [notifications, setNotifications] = useState(3);

  const { data: vehicles } = useQuery<Vehicle[]>({
    queryKey: ['vehicles'],
    queryFn: () => api.get('/vehicles').then(r => r.data.vehicles),
  });

  const { data: activities } = useQuery<Activity[]>({
    queryKey: ['activities'],
    queryFn: () => api.get('/activities/recent').then(r => r.data),
  });

  const quickActions = [
    { icon: <LockOpen />, label: '解锁', color: '#4caf50' },
    { icon: <Lock />, label: '锁车', color: '#f44336' },
    { icon: <TrackChanges />, label: '寻车', color: '#2196f3' },
  ];

  return (
    <Box sx={{ pb: 8 }}>
      {/* 头部欢迎区 */}
      <Box sx={{ p: 2, pt: 3 }}>
        <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <Box>
            <Typography variant="body2" color="text.secondary">
              欢迎回来，
            </Typography>
            <Typography variant="h5" fontWeight="bold">
              {user?.name || '用户'}
            </Typography>
          </Box>
          <Box sx={{ display: 'flex', gap: 1 }}>
            <IconButton>
              <Badge badgeContent={notifications} color="error">
                <Notifications />
              </Badge>
            </IconButton>
            <IconButton onClick={() => navigate('/settings')}>
              <Settings />
            </IconButton>
          </Box>
        </Box>
      </Box>

      {/* 车辆卡片 */}
      <Box sx={{ px: 2, py: 1 }}>
        <Typography variant="subtitle1" fontWeight="bold" sx={{ mb: 2 }}>
          我的车辆
        </Typography>
        
        {vehicles?.map((vehicle, index) => (
          <motion.div
            key={vehicle.id}
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
            transition={{ delay: index * 0.1 }}
          >
            <Card 
              sx={{ 
                mb: 2, 
                borderRadius: 3,
                background: 'linear-gradient(135deg, #667eea 0%, #764ba2 100%)',
                color: 'white'
              }}
              onClick={() => navigate(`/vehicles/${vehicle.id}`)}
            >
              <CardContent>
                <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                  <Box>
                    <Typography variant="h6" fontWeight="bold">
                      {vehicle.brand} {vehicle.model}
                    </Typography>
                    <Typography variant="body2" sx={{ opacity: 0.9 }}>
                      {vehicle.year} 款
                    </Typography>
                  </Box>
                  <Chip 
                    label={vehicle.status === 'online' ? '在线' : '离线'}
                    size="small"
                    sx={{ 
                      bgcolor: vehicle.status === 'online' ? 'rgba(76, 175, 80, 0.9)' : 'rgba(158, 158, 158, 0.9)',
                      color: 'white',
                      fontWeight: 'bold'
                    }}
                  />
                </Box>

                <Box sx={{ mt: 3 }}>
                  <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                    <DirectionsCar sx={{ fontSize: 18 }} />
                    <Typography variant="body2">
                      电量 {vehicle.battery_level}%
                    </Typography>
                  </Box>
                  <LinearProgress 
                    variant="determinate" 
                    value={vehicle.battery_level}
                    sx={{ 
                      mt: 1, 
                      height: 6, 
                      borderRadius: 3,
                      bgcolor: 'rgba(255,255,255,0.2)',
                      '& .MuiLinearProgress-bar': {
                        bgcolor: vehicle.battery_level > 20 ? '#4caf50' : '#ff9800'
                      }
                    }}
                  />
                </Box>

                {/* 快捷操作 */}
                <Box sx={{ display: 'flex', gap: 2, mt: 3, justifyContent: 'center' }}>
                  {quickActions.map((action) => (
                    <Box key={action.label} sx={{ textAlign: 'center' }}>
                      <Fab 
                        size="small" 
                        sx={{ 
                          bgcolor: 'rgba(255,255,255,0.2)',
                          color: 'white',
                          '&:hover': { bgcolor: 'rgba(255,255,255,0.3)' }
                        }}
                      >
                        {action.icon}
                      </Fab>
                      <Typography variant="caption" display="block" sx={{ mt: 0.5 }}>
                        {action.label}
                      </Typography>
                    </Box>
                  ))}
                </Box>
              </CardContent>
            </Card>
          </motion.div>
        ))}

        {/* 添加车辆按钮 */}
        <Button
          fullWidth
          variant="outlined"
          startIcon={<Add />}
          sx={{ borderRadius: 3, py: 1.5, borderStyle: 'dashed' }}
          onClick={() => navigate('/vehicles/add')}
        >
          添加车辆
        </Button>
      </Box>

      {/* 活动日志 */}
      <Box sx={{ px: 2, mt: 3 }}>
        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
          <Typography variant="subtitle1" fontWeight="bold">
            最近活动
          </Typography>
          <Button size="small" endIcon={<ChevronRight />}>
            查看全部
          </Button>
        </Box>

        <Card sx={{ borderRadius: 3 }}>
          <List sx={{ py: 0 }}>
            {activities?.slice(0, 5).map((activity, index) => (
              <ListItem 
                key={activity.id}
                divider={index < (activities?.length || 0) - 1}
                sx={{ py: 1.5 }}
              >
                <ListItemAvatar>
                  <Avatar sx={{ bgcolor: 'primary.light' }}>
                    <DirectionsCar />
                  </Avatar>
                </ListItemAvatar>
                <ListItemText
                  primary={activity.message}
                  secondary={`${activity.vehicle} · ${new Date(activity.timestamp).toLocaleString('zh-CN')}`}
                  primaryTypographyProps={{ fontWeight: 'medium' }}
                />
              </ListItem>
            )) || (
              <ListItem>
                <ListItemText 
                  primary="暂无活动" 
                  secondary="您的车辆活动将显示在这里"
                />
              </ListItem>
            )}
          </List>
        </Card>
      </Box>
    </Box>
  );
}
