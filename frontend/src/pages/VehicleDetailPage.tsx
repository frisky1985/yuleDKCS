import { useState } from 'react'
import { useParams, useNavigate } from 'react-router-dom'
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query'
import {
  Box, Typography, Paper, Grid, Chip, Button, IconButton,
  Tabs, Tab, Divider, List, ListItem, ListItemText, ListItemIcon,
  Dialog, DialogTitle, DialogContent, DialogActions,
  CircularProgress, Alert, Avatar, LinearProgress, Fab,
} from '@mui/material'
import {
  ArrowBack, Edit, Delete, DirectionsCar,
  Lock, LockOpen, PowerSettingsNew, Motorcycle,
  LocalShipping, AcUnit, VolumeUp, FlashOn,
  LocationOn, Key, AccessTime, History,
  CheckCircle, Error as ErrorIcon, SignalCellularAlt,
  Battery90, Speed,
} from '@mui/icons-material'
import { motion } from 'framer-motion'
import { vehiclesApi, type Vehicle, type VehicleStatus, type VehicleCommand } from '../api/vehicles'
import { format } from 'date-fns'
import { zhCN } from 'date-fns/locale'

// 车辆状态中文映射
const statusLabels: Record<string, { label: string; color: 'success' | 'warning' | 'error' | 'default' }> = {
  online: { label: '在线', color: 'success' },
  offline: { label: '离线', color: 'error' },
  sleeping: { label: '休眠', color: 'warning' },
  unknown: { label: '未知', color: 'default' },
}

const lockStateLabels: Record<string, { label: string; icon: React.ReactNode }> = {
  locked: { label: '已锁定', icon: <Lock sx={{ fontSize: 20 }} /> },
  unlocked: { label: '未锁定', icon: <LockOpen sx={{ fontSize: 20 }} /> },
  unknown: { label: '未知', icon: <Lock sx={{ fontSize: 20, opacity: 0.4 }} /> },
}

// 快捷控制命令
interface QuickAction {
  command: VehicleCommand
  label: string
  icon: React.ReactNode
  color: string
}

const quickActions: QuickAction[] = [
  { command: 'unlock', label: '解锁', icon: <LockOpen />, color: '#4caf50' },
  { command: 'lock', label: '锁车', icon: <Lock />, color: '#f44336' },
  { command: 'engine_start', label: '启动', icon: <PowerSettingsNew />, color: '#ff9800' },
  { command: 'engine_stop', label: '熄火', icon: <PowerSettingsNew />, color: '#9e9e9e' },
  { command: 'trunk_open', label: '开后备箱', icon: <LocalShipping />, color: '#2196f3' },
  { command: 'horn', label: '鸣笛', icon: <VolumeUp />, color: '#9c27b0' },
  { command: 'lights', label: '闪灯', icon: <FlashOn />, color: '#ffeb3b' },
  { command: 'locate', label: '寻车', icon: <LocationOn />, color: '#00bcd4' },
]

export default function VehicleDetailPage() {
  const { vehicleId } = useParams<{ vehicleId: string }>()
  const navigate = useNavigate()
  const queryClient = useQueryClient()
  const [activeTab, setActiveTab] = useState(0)
  const [showDeleteDialog, setShowDeleteDialog] = useState(false)
  const [showCommandResult, setShowCommandResult] = useState<{ open: boolean; message: string; success: boolean }>({
    open: false,
    message: '',
    success: true,
  })

  const id = vehicleId ? parseInt(vehicleId) : undefined

  // 获取车辆详情
  const { data: vehicleData, isLoading, error } = useQuery({
    queryKey: ['vehicle', id],
    queryFn: () => vehiclesApi.getVehicleDetail(id!),
    enabled: !!id,
  })

  // 获取车辆状态 (30秒轮询)
  const { data: statusData } = useQuery({
    queryKey: ['vehicleStatus', id],
    queryFn: () => vehiclesApi.getVehicleStatus(id!),
    enabled: !!id,
    refetchInterval: 30000,
  })

  // 获取关联钥匙
  const { data: vehicleKeys } = useQuery({
    queryKey: ['vehicleKeys', id],
    queryFn: () => vehiclesApi.getVehicleKeys(id!),
    enabled: !!id && activeTab === 1,
  })

  // 发送控制命令
  const commandMutation = useMutation({
    mutationFn: ({ command }: { command: VehicleCommand }) =>
      vehiclesApi.sendCommand(id!, command),
    onSuccess: (response) => {
      const data = response?.data || response
      setShowCommandResult({
        open: true,
        message: data?.status === 'success'
          ? '命令执行成功'
          : data?.status === 'pending'
            ? '命令已发送，等待执行'
            : `命令执行失败: ${data?.error || '未知错误'}`,
        success: data?.status === 'success' || data?.status === 'pending',
      })
      queryClient.invalidateQueries({ queryKey: ['vehicleStatus', id] })
    },
    onError: () => {
      setShowCommandResult({
        open: true,
        message: '命令发送失败，请检查车辆连接状态',
        success: false,
      })
    },
  })

  // 删除车辆
  const deleteMutation = useMutation({
    mutationFn: () => vehiclesApi.deleteVehicle(id!),
    onSuccess: () => {
      navigate('/vehicles')
    },
  })

  // 提取数据
  const vehicle: Vehicle | undefined = vehicleData?.data || vehicleData
  const status: VehicleStatus | undefined = statusData?.data || statusData
  const keys: any[] = vehicleKeys?.data || vehicleKeys || []

  if (isLoading) {
    return (
      <Box display="flex" justifyContent="center" alignItems="center" minHeight="60vh">
        <CircularProgress />
      </Box>
    )
  }

  if (error || !vehicle) {
    return (
      <Box p={3}>
        <Alert severity="error" action={
          <Button color="inherit" size="small" onClick={() => navigate('/vehicles')}>
            返回车辆列表
          </Button>
        }>
          加载车辆详情失败，请稍后重试
        </Alert>
      </Box>
    )
  }

  const statusInfo = statusLabels[vehicle.status] || statusLabels.unknown
  const lockInfo = lockStateLabels[status?.lock_state || 'unknown']
  const batteryColor = vehicle.battery_level > 60 ? '#4caf50'
    : vehicle.battery_level > 20 ? '#ff9800' : '#f44336'
  const isOnline = vehicle.status === 'online'

  return (
    <Box sx={{ pb: 4 }}>
      {/* 顶部导航 */}
      <Box sx={{ p: 2, display: 'flex', alignItems: 'center', gap: 2 }}>
        <IconButton onClick={() => navigate('/vehicles')}>
          <ArrowBack />
        </IconButton>
        <Typography variant="h5" fontWeight="bold" sx={{ flex: 1 }}>
          车辆详情
        </Typography>
        <IconButton onClick={() => navigate(`/vehicles/${vehicleId}/edit`)}>
          <Edit />
        </IconButton>
        <IconButton color="error" onClick={() => setShowDeleteDialog(true)}>
          <Delete />
        </IconButton>
      </Box>

      <Box sx={{ px: 3 }}>
        <Grid container spacing={3}>
          {/* ===== 左侧信息卡片 ===== */}
          <Grid item xs={12} md={4}>
            <motion.div
              initial={{ opacity: 0, y: 20 }}
              animate={{ opacity: 1, y: 0 }}
            >
              <Paper elevation={2} sx={{ p: 3, borderRadius: 3 }}>
                {/* 车辆图标 */}
                <Box sx={{ textAlign: 'center', mb: 3 }}>
                  <Box
                    sx={{
                      width: 88,
                      height: 88,
                      borderRadius: '50%',
                      bgcolor: 'primary.main',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      mx: 'auto',
                      mb: 2,
                      boxShadow: 2,
                    }}
                  >
                    <DirectionsCar sx={{ fontSize: 44, color: 'white' }} />
                  </Box>
                  <Typography variant="h6" fontWeight="bold">
                    {vehicle.brand} {vehicle.model}
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    {vehicle.plate_number || vehicle.vin}
                  </Typography>
                  <Box sx={{ mt: 1, display: 'flex', gap: 1, justifyContent: 'center' }}>
                    <Chip
                      label={statusInfo.label}
                      size="small"
                      color={statusInfo.color}
                      sx={{ fontWeight: 'bold' }}
                    />
                    {status && (
                      <Chip
                        label={lockInfo.label}
                        size="small"
                        variant="outlined"
                        icon={lockInfo.icon as React.ReactElement}
                      />
                    )}
                  </Box>
                </Box>

                <Divider sx={{ my: 2 }} />

                {/* 车辆信息列表 */}
                <Box sx={{ display: 'flex', flexDirection: 'column', gap: 1.5 }}>
                  <DetailRow label="车架号 (VIN)" value={vehicle.vin} />
                  <DetailRow label="年份" value={`${vehicle.year} 款`} />
                  {vehicle.color && <DetailRow label="颜色" value={vehicle.color} />}
                  {vehicle.plate_number && <DetailRow label="车牌号" value={vehicle.plate_number} />}
                  <DetailRow label="添加时间" value={
                    format(new Date(vehicle.created_at), 'yyyy年MM月dd日', { locale: zhCN })
                  } />
                </Box>

                <Divider sx={{ my: 2 }} />

                {/* 实时状态 */}
                <Typography variant="subtitle2" fontWeight="bold" sx={{ mb: 1.5 }}>
                  实时状态
                </Typography>

                <Box sx={{ mb: 2 }}>
                  <Box sx={{ display: 'flex', alignItems: 'center', gap: 0.5, mb: 0.5 }}>
                    <Battery90 sx={{ fontSize: 18, color: batteryColor }} />
                    <Typography variant="body2" fontWeight="medium">
                      电量 {vehicle.battery_level}%
                    </Typography>
                  </Box>
                  <LinearProgress
                    variant="determinate"
                    value={vehicle.battery_level}
                    sx={{
                      height: 6,
                      borderRadius: 3,
                      bgcolor: 'action.hover',
                      '& .MuiLinearProgress-bar': { bgcolor: batteryColor },
                    }}
                  />
                </Box>

                {status?.fuel_level !== undefined && (
                  <Box sx={{ mb: 2 }}>
                    <Box sx={{ display: 'flex', alignItems: 'center', gap: 0.5, mb: 0.5 }}>
                      <Speed sx={{ fontSize: 18, color: 'text.secondary' }} />
                      <Typography variant="body2">油量 {status.fuel_level}%</Typography>
                    </Box>
                    <LinearProgress
                      variant="determinate"
                      value={status.fuel_level}
                      sx={{ height: 6, borderRadius: 3, bgcolor: 'action.hover' }}
                    />
                  </Box>
                )}

                {status?.last_updated && (
                  <Box sx={{ display: 'flex', alignItems: 'center', gap: 0.5 }}>
                    <AccessTime sx={{ fontSize: 14, color: 'text.secondary' }} />
                    <Typography variant="caption" color="text.secondary">
                      最后更新: {format(new Date(status.last_updated), 'HH:mm:ss')}
                    </Typography>
                  </Box>
                )}
              </Paper>
            </motion.div>
          </Grid>

          {/* ===== 右侧内容 ===== */}
          <Grid item xs={12} md={8}>
            {/* 快捷控制面板 */}
            <motion.div
              initial={{ opacity: 0, y: 20 }}
              animate={{ opacity: 1, y: 0 }}
              transition={{ delay: 0.1 }}
            >
              <Paper elevation={2} sx={{ p: 3, borderRadius: 3, mb: 3 }}>
                <Typography variant="subtitle1" fontWeight="bold" sx={{ mb: 2 }}>
                  快捷控制
                </Typography>
                <Grid container spacing={1}>
                  {quickActions.map((action) => (
                    <Grid item xs={3} sm={1.5} key={action.command}>
                      <Box sx={{ textAlign: 'center' }}>
                        <Fab
                          size="medium"
                          sx={{
                            bgcolor: isOnline ? `${action.color}20` : 'action.hover',
                            color: isOnline ? action.color : 'text.disabled',
                            '&:hover': isOnline ? { bgcolor: `${action.color}40` } : {},
                            transition: 'all 0.2s',
                          }}
                          disabled={!isOnline || commandMutation.isPending}
                          onClick={() => commandMutation.mutate({ command: action.command })}
                        >
                          {action.icon}
                        </Fab>
                        <Typography
                          variant="caption"
                          display="block"
                          sx={{ mt: 0.5, fontSize: 11, color: isOnline ? 'text.primary' : 'text.disabled' }}
                        >
                          {action.label}
                        </Typography>
                      </Box>
                    </Grid>
                  ))}
                </Grid>
              </Paper>
            </motion.div>

            {/* 选项卡面板 */}
            <Paper elevation={2} sx={{ borderRadius: 3 }}>
              <Tabs
                value={activeTab}
                onChange={(_, v) => setActiveTab(v)}
                sx={{ borderBottom: 1, borderColor: 'divider' }}
              >
                <Tab label="位置与状态" icon={<LocationOn />} iconPosition="start" />
                <Tab label="关联钥匙" icon={<Key />} iconPosition="start" />
              </Tabs>

              {/* 位置与状态 */}
              {activeTab === 0 && (
                <Box sx={{ p: 3 }}>
                  <Typography variant="h6" gutterBottom>
                    车辆位置
                  </Typography>
                  {vehicle.location || status?.location ? (
                    <Box sx={{ bgcolor: 'grey.100', borderRadius: 2, p: 3, textAlign: 'center', mb: 3 }}>
                      <LocationOn sx={{ fontSize: 48, color: 'primary.main', mb: 1 }} />
                      <Typography variant="body1" fontWeight="medium">
                        {status?.location?.latitude.toFixed(6) || vehicle.location?.latitude.toFixed(6)},
                        {' '}
                        {status?.location?.longitude.toFixed(6) || vehicle.location?.longitude.toFixed(6)}
                      </Typography>
                      <Typography variant="caption" color="text.secondary">
                        (地图组件可在此集成)
                      </Typography>
                    </Box>
                  ) : (
                    <Box sx={{ bgcolor: 'grey.100', borderRadius: 2, p: 4, textAlign: 'center', mb: 3 }}>
                      <LocationOn sx={{ fontSize: 48, color: 'text.disabled', mb: 1 }} />
                      <Typography color="text.secondary">暂无位置信息</Typography>
                    </Box>
                  )}

                  <Typography variant="h6" gutterBottom>
                    详细状态
                  </Typography>
                  <Grid container spacing={2}>
                    <Grid item xs={6}>
                      <StateCard
                        label="连接状态"
                        value={statusInfo.label}
                        color={statusInfo.color}
                      />
                    </Grid>
                    <Grid item xs={6}>
                      <StateCard
                        label="门锁状态"
                        value={lockInfo.label}
                        icon={lockInfo.icon as React.ReactElement}
                      />
                    </Grid>
                    <Grid item xs={6}>
                      <StateCard
                        label="引擎状态"
                        value={status?.engine_state === 'running' ? '运行中' : status?.engine_state === 'stopped' ? '已停止' : '未知'}
                        color={status?.engine_state === 'running' ? 'success' : 'default'}
                      />
                    </Grid>
                    <Grid item xs={6}>
                      <StateCard
                        label="电量"
                        value={`${vehicle.battery_level}%`}
                        color={vehicle.battery_level > 60 ? 'success' : vehicle.battery_level > 20 ? 'warning' : 'error'}
                      />
                    </Grid>
                  </Grid>
                </Box>
              )}

              {/* 关联钥匙 */}
              {activeTab === 1 && (
                <Box sx={{ p: 3 }}>
                  <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', mb: 2 }}>
                    <Typography variant="h6">
                      关联钥匙 ({keys.length || 0})
                    </Typography>
                    <Button
                      variant="outlined"
                      size="small"
                      startIcon={<Key />}
                      onClick={() => navigate('/keys')}
                      sx={{ borderRadius: 2 }}
                    >
                      管理钥匙
                    </Button>
                  </Box>

                  {keys.length === 0 ? (
                    <Box sx={{ textAlign: 'center', py: 6 }}>
                      <Key sx={{ fontSize: 48, color: 'text.disabled', mb: 1 }} />
                      <Typography color="text.secondary">
                        此车辆暂无关联的数字钥匙
                      </Typography>
                    </Box>
                  ) : (
                    <List>
                      {(Array.isArray(keys) ? keys : []).map((key: any, index: number) => (
                        <ListItem
                          key={key.id || index}
                          divider
                          sx={{ cursor: 'pointer', borderRadius: 2, '&:hover': { bgcolor: 'action.hover' } }}
                          onClick={() => navigate(`/keys/${key.id}`)}
                        >
                          <ListItemIcon>
                            <Avatar sx={{ bgcolor: 'primary.main', width: 36, height: 36 }}>
                              <Key />
                            </Avatar>
                          </ListItemIcon>
                          <ListItemText
                            primary={
                              <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                                <Typography variant="body2" fontWeight="medium">
                                  {key.type || key.protocol || '数字钥匙'}
                                </Typography>
                                <Chip
                                  label={key.status === 'active' ? '有效' : key.status === 'expired' ? '已过期' : '已停用'}
                                  size="small"
                                  color={key.status === 'active' ? 'success' : 'default'}
                                />
                              </Box>
                            }
                            secondary={`类型: ${key.key_type || key.type || 'owner'} · 创建于 ${
                              key.created_at
                                ? format(new Date(key.created_at), 'yyyy-MM-dd')
                                : '未知'
                            }`}
                          />
                        </ListItem>
                      ))}
                    </List>
                  )}
                </Box>
              )}
            </Paper>
          </Grid>
        </Grid>
      </Box>

      {/* 命令结果对话框 */}
      <Dialog open={showCommandResult.open} onClose={() => setShowCommandResult({ ...showCommandResult, open: false })}>
        <DialogTitle>
          {showCommandResult.success ? '操作成功' : '操作失败'}
        </DialogTitle>
        <DialogContent>
          <Box sx={{ display: 'flex', alignItems: 'center', gap: 2 }}>
            {showCommandResult.success ? (
              <CheckCircle color="success" sx={{ fontSize: 40 }} />
            ) : (
              <ErrorIcon color="error" sx={{ fontSize: 40 }} />
            )}
            <Typography>{showCommandResult.message}</Typography>
          </Box>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setShowCommandResult({ ...showCommandResult, open: false })}>
            确定
          </Button>
        </DialogActions>
      </Dialog>

      {/* 删除确认对话框 */}
      <Dialog open={showDeleteDialog} onClose={() => setShowDeleteDialog(false)}>
        <DialogTitle>确认删除车辆</DialogTitle>
        <DialogContent>
          <Typography>
            确定要删除 <strong>{vehicle.brand} {vehicle.model}</strong> 吗？
            此操作将同时删除关联的所有数字钥匙，且不可撤销。
          </Typography>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setShowDeleteDialog(false)}>取消</Button>
          <Button
            onClick={() => deleteMutation.mutate()}
            color="error"
            variant="contained"
            disabled={deleteMutation.isPending}
          >
            {deleteMutation.isPending ? '删除中...' : '确认删除'}
          </Button>
        </DialogActions>
      </Dialog>
    </Box>
  )
}

// =============================================================================
// 辅助组件
// =============================================================================

/** 详情行：标签 + 值 */
function DetailRow({ label, value }: { label: string; value: string }) {
  return (
    <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
      <Typography variant="body2" color="text.secondary">
        {label}
      </Typography>
      <Typography variant="body2" fontWeight="medium">
        {value}
      </Typography>
    </Box>
  )
}

/** 状态卡片 */
function StateCard({
  label,
  value,
  color,
  icon,
}: {
  label: string
  value: string
  color?: 'success' | 'warning' | 'error' | 'default'
  icon?: React.ReactElement
}) {
  return (
    <Paper variant="outlined" sx={{ p: 2, borderRadius: 2 }}>
      <Typography variant="caption" color="text.secondary" sx={{ mb: 0.5, display: 'block' }}>
        {label}
      </Typography>
      <Box sx={{ display: 'flex', alignItems: 'center', gap: 0.5 }}>
        {icon && <Box sx={{ display: 'flex' }}>{icon}</Box>}
        <Typography variant="body2" fontWeight="bold" color={color ? `${color}.main` : 'text.primary'}>
          {value}
        </Typography>
      </Box>
    </Paper>
  )
}
