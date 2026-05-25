import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query'
import {
  Box, Typography, Card, CardContent, Grid, Chip, Button, IconButton,
  Dialog, DialogTitle, DialogContent, DialogActions, TextField,
  CircularProgress, Alert, LinearProgress, Tooltip, Fab,
} from '@mui/material'
import {
  DirectionsCar, Add, Delete, Edit, ChevronRight,
  Battery90, SignalCellularAlt, LocationOn,
  Lock, LockOpen, MoreVert,
} from '@mui/icons-material'
import { motion } from 'framer-motion'
import { vehiclesApi, type Vehicle } from '../api/vehicles'

// 车辆状态中文映射
const statusLabels: Record<string, { label: string; color: 'success' | 'warning' | 'error' | 'default' }> = {
  online: { label: '在线', color: 'success' },
  offline: { label: '离线', color: 'error' },
  sleeping: { label: '休眠', color: 'warning' },
  unknown: { label: '未知', color: 'default' },
}

export default function VehiclesPage() {
  const navigate = useNavigate()
  const queryClient = useQueryClient()
  const [showAddDialog, setShowAddDialog] = useState(false)
  const [showDeleteDialog, setShowDeleteDialog] = useState<{ open: boolean; vehicleId?: number; name?: string }>({
    open: false,
  })
  const [formData, setFormData] = useState({
    vin: '',
    brand: '',
    model: '',
    year: new Date().getFullYear(),
    color: '',
    plate_number: '',
  })

  // 获取车辆列表
  const { data, isLoading, error } = useQuery({
    queryKey: ['vehicles'],
    queryFn: () => vehiclesApi.getMyVehicles(),
  })

  // 删除车辆
  const deleteMutation = useMutation({
    mutationFn: (vehicleId: number) => vehiclesApi.deleteVehicle(vehicleId),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['vehicles'] })
      setShowDeleteDialog({ open: false })
    },
  })

  // 添加车辆
  const createMutation = useMutation({
    mutationFn: () => vehiclesApi.createVehicle({
      vin: formData.vin,
      brand: formData.brand,
      model: formData.model,
      year: formData.year,
      color: formData.color,
      plate_number: formData.plate_number,
    }),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['vehicles'] })
      setShowAddDialog(false)
      setFormData({
        vin: '', brand: '', model: '', year: new Date().getFullYear(),
        color: '', plate_number: '',
      })
    },
  })

  const vehicles: Vehicle[] = data?.data?.vehicles || data?.vehicles || []

  // 重置添加表单
  const resetForm = () => {
    setFormData({
      vin: '', brand: '', model: '', year: new Date().getFullYear(),
      color: '', plate_number: '',
    })
  }

  if (isLoading) {
    return (
      <Box display="flex" justifyContent="center" alignItems="center" minHeight="60vh">
        <CircularProgress />
      </Box>
    )
  }

  if (error) {
    return (
      <Box p={3}>
        <Alert severity="error">加载车辆列表失败，请稍后重试</Alert>
      </Box>
    )
  }

  return (
    <Box sx={{ pb: 8 }}>
      {/* 页面标题 */}
      <Box sx={{ p: 2, pt: 3, display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <Typography variant="h5" fontWeight="bold">
          我的车辆
        </Typography>
        <Button
          variant="contained"
          startIcon={<Add />}
          onClick={() => { resetForm(); setShowAddDialog(true) }}
          sx={{ borderRadius: 2 }}
        >
          添加车辆
        </Button>
      </Box>

      {/* 车辆列表 */}
      <Box sx={{ px: 2 }}>
        {vehicles.length === 0 ? (
          <Box sx={{ textAlign: 'center', py: 8 }}>
            <DirectionsCar sx={{ fontSize: 64, color: 'text.disabled', mb: 2 }} />
            <Typography variant="h6" color="text.secondary" gutterBottom>
              还没有添加车辆
            </Typography>
            <Typography variant="body2" color="text.disabled" sx={{ mb: 3 }}>
              添加您的第一辆车，开始使用数字钥匙功能
            </Typography>
            <Button
              variant="outlined"
              startIcon={<Add />}
              onClick={() => { resetForm(); setShowAddDialog(true) }}
              sx={{ borderRadius: 2 }}
            >
              添加车辆
            </Button>
          </Box>
        ) : (
          <Grid container spacing={2}>
            {vehicles.map((vehicle, index) => {
              const statusInfo = statusLabels[vehicle.status] || statusLabels.unknown
              const batteryColor = vehicle.battery_level > 60 ? '#4caf50'
                : vehicle.battery_level > 20 ? '#ff9800' : '#f44336'

              return (
                <Grid item xs={12} sm={6} key={vehicle.id}>
                  <motion.div
                    initial={{ opacity: 0, y: 20 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ delay: index * 0.08 }}
                  >
                    <Card
                      sx={{
                        borderRadius: 3,
                        cursor: 'pointer',
                        transition: 'all 0.2s ease',
                        '&:hover': {
                          transform: 'translateY(-2px)',
                          boxShadow: 4,
                        },
                      }}
                      onClick={() => navigate(`/vehicles/${vehicle.id}`)}
                    >
                      <CardContent>
                        {/* 顶部: 品牌型号 + 状态 */}
                        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', mb: 2 }}>
                          <Box sx={{ display: 'flex', alignItems: 'center', gap: 1.5 }}>
                            <Box
                              sx={{
                                width: 44,
                                height: 44,
                                borderRadius: 2,
                                bgcolor: 'primary.main',
                                display: 'flex',
                                alignItems: 'center',
                                justifyContent: 'center',
                              }}
                            >
                              <DirectionsCar sx={{ color: 'white', fontSize: 24 }} />
                            </Box>
                            <Box>
                              <Typography variant="subtitle1" fontWeight="bold" lineHeight={1.2}>
                                {vehicle.brand} {vehicle.model}
                              </Typography>
                              <Typography variant="caption" color="text.secondary">
                                {vehicle.plate_number || vehicle.vin?.slice(-6) || `${vehicle.year}款`}
                              </Typography>
                            </Box>
                          </Box>
                          <Chip
                            label={statusInfo.label}
                            size="small"
                            color={statusInfo.color}
                            sx={{ fontWeight: 'bold', fontSize: 12 }}
                          />
                        </Box>

                        {/* 电量 */}
                        <Box sx={{ mb: 2 }}>
                          <Box sx={{ display: 'flex', alignItems: 'center', gap: 0.5, mb: 0.5 }}>
                            <Battery90 sx={{ fontSize: 16, color: batteryColor }} />
                            <Typography variant="caption" color={batteryColor} fontWeight="medium">
                              电量 {vehicle.battery_level}%
                            </Typography>
                          </Box>
                          <LinearProgress
                            variant="determinate"
                            value={vehicle.battery_level}
                            sx={{
                              height: 4,
                              borderRadius: 2,
                              bgcolor: 'action.hover',
                              '& .MuiLinearProgress-bar': { bgcolor: batteryColor },
                            }}
                          />
                        </Box>

                        {/* 底部: 位置 + 操作 */}
                        <Box sx={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                          <Box sx={{ display: 'flex', alignItems: 'center', gap: 0.5 }}>
                            <LocationOn sx={{ fontSize: 14, color: 'text.secondary' }} />
                            <Typography variant="caption" color="text.secondary">
                              {vehicle.location
                                ? `${vehicle.location.latitude.toFixed(4)}, ${vehicle.location.longitude.toFixed(4)}`
                                : '位置未知'}
                            </Typography>
                          </Box>
                          <Box sx={{ display: 'flex', gap: 0.5 }} onClick={(e) => e.stopPropagation()}>
                            <Tooltip title="编辑">
                              <IconButton
                                size="small"
                                onClick={() => navigate(`/vehicles/${vehicle.id}/edit`)}
                              >
                                <Edit fontSize="small" />
                              </IconButton>
                            </Tooltip>
                            <Tooltip title="删除">
                              <IconButton
                                size="small"
                                color="error"
                                onClick={() => setShowDeleteDialog({
                                  open: true,
                                  vehicleId: vehicle.id,
                                  name: `${vehicle.brand} ${vehicle.model}`,
                                })}
                              >
                                <Delete fontSize="small" />
                              </IconButton>
                            </Tooltip>
                            <IconButton size="small">
                              <ChevronRight fontSize="small" />
                            </IconButton>
                          </Box>
                        </Box>
                      </CardContent>
                    </Card>
                  </motion.div>
                </Grid>
              )
            })}
          </Grid>
        )}
      </Box>

      {/* 添加车辆对话框 */}
      <Dialog open={showAddDialog} onClose={() => setShowAddDialog(false)} maxWidth="sm" fullWidth>
        <DialogTitle>添加车辆</DialogTitle>
        <DialogContent>
          <Box sx={{ display: 'flex', flexDirection: 'column', gap: 2, pt: 1 }}>
            <TextField
              fullWidth
              required
              label="车架号 (VIN)"
              placeholder="17位车架号"
              value={formData.vin}
              onChange={(e) => setFormData({ ...formData, vin: e.target.value })}
            />
            <Box sx={{ display: 'flex', gap: 2 }}>
              <TextField
                fullWidth
                required
                label="品牌"
                placeholder="例如: Tesla"
                value={formData.brand}
                onChange={(e) => setFormData({ ...formData, brand: e.target.value })}
              />
              <TextField
                fullWidth
                required
                label="型号"
                placeholder="例如: Model 3"
                value={formData.model}
                onChange={(e) => setFormData({ ...formData, model: e.target.value })}
              />
            </Box>
            <Box sx={{ display: 'flex', gap: 2 }}>
              <TextField
                fullWidth
                label="年份"
                type="number"
                value={formData.year}
                onChange={(e) => setFormData({ ...formData, year: parseInt(e.target.value) || new Date().getFullYear() })}
              />
              <TextField
                fullWidth
                label="颜色"
                placeholder="例如: 珍珠白"
                value={formData.color}
                onChange={(e) => setFormData({ ...formData, color: e.target.value })}
              />
            </Box>
            <TextField
              fullWidth
              label="车牌号"
              placeholder="例如: 京A12345"
              value={formData.plate_number}
              onChange={(e) => setFormData({ ...formData, plate_number: e.target.value })}
            />
          </Box>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setShowAddDialog(false)}>取消</Button>
          <Button
            variant="contained"
            onClick={() => createMutation.mutate()}
            disabled={!formData.vin || !formData.brand || !formData.model || createMutation.isPending}
          >
            {createMutation.isPending ? '添加中...' : '确认添加'}
          </Button>
        </DialogActions>
      </Dialog>

      {/* 删除确认对话框 */}
      <Dialog open={showDeleteDialog.open} onClose={() => setShowDeleteDialog({ open: false })}>
        <DialogTitle>确认删除车辆</DialogTitle>
        <DialogContent>
          <Typography>
            确定要删除 <strong>{showDeleteDialog.name}</strong> 吗？
            删除后关联的数字钥匙将同时失效，此操作不可撤销。
          </Typography>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setShowDeleteDialog({ open: false })}>取消</Button>
          <Button
            onClick={() => showDeleteDialog.vehicleId && deleteMutation.mutate(showDeleteDialog.vehicleId)}
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
