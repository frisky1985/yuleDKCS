import { useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import {
  Box, Typography, Paper, Grid, Chip, Button, IconButton,
  Tabs, Tab, Divider, List, ListItem, ListItemText, ListItemIcon,
  Dialog, DialogTitle, DialogContent, DialogActions,
  Switch, FormControlLabel, TextField, MenuItem, Select, FormControl,
  InputLabel, CircularProgress, Alert, Tooltip
} from '@mui/material';
import {
  ArrowBack, Edit, Share, Delete, Pause, Play,
  LockOpen, Lock, DirectionsCar, AccessTime,
  LocationOn, QrCode, History, Settings,
  CheckCircle, Warning, Error as ErrorIcon
} from '@mui/icons-material';
import { motion } from 'framer-motion';
import { keysApi, type DigitalKey, type KeyPermission, type KeyUsageLog } from '../api/keys';
import { format } from 'date-fns';
import { zhCN } from 'date-fns/locale';

const permissionLabels: Record<string, string> = {
  unlock: '解锁',
  lock: '锁车',
  start_engine: '启动引擎',
  stop_engine: '停止引擎',
  open_trunk: '开后备箱',
  open_windows: '开窗',
  close_windows: '关窗',
  find_vehicle: '寻车',
};

const protocolColors: Record<string, string> = {
  CCC: '#2196f3',
  ICCOA: '#ff9800',
  ICCE: '#4caf50',
};

export default function KeyDetailPage() {
  const { keyId } = useParams<{ keyId: string }>();
  const navigate = useNavigate();
  const queryClient = useQueryClient();
  const [activeTab, setActiveTab] = useState(0);
  const [showShareDialog, setShowShareDialog] = useState(false);
  const [showDeactivateDialog, setShowDeactivateDialog] = useState(false);

  const { data: keyDetail, isLoading, error } = useQuery<DigitalKey>({
    queryKey: ['key', keyId],
    queryFn: () => keysApi.getKeyDetail(keyId!),
    enabled: !!keyId,
  });

  const { data: usageLogs } = useQuery<{ logs: KeyUsageLog[]; total: number }>({
    queryKey: ['keyLogs', keyId],
    queryFn: () => keysApi.getUsageLogs(keyId!, { limit: 50 }),
    enabled: !!keyId && activeTab === 1,
  });

  const deactivateMutation = useMutation({
    mutationFn: () => keysApi.deactivateKey(keyId!),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['key', keyId] });
      setShowDeactivateDialog(false);
    },
  });

  const activateMutation = useMutation({
    mutationFn: () => keysApi.activateKey(keyId!),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['key', keyId] }),
  });

  const revokeMutation = useMutation({
    mutationFn: () => keysApi.revokeKey(keyId!),
    onSuccess: () => navigate('/keys'),
  });

  if (isLoading) {
    return (
      <Box display="flex" justifyContent="center" alignItems="center" minHeight="60vh">
        <CircularProgress />
      </Box>
    );
  }

  if (error || !keyDetail) {
    return (
      <Box p={3}>
        <Alert severity="error">加载钥匙详情失败，请稍后重试</Alert>
      </Box>
    );
  }

  const isActive = keyDetail.status === 'active';
  const isExpired = keyDetail.status === 'expired';
  const isOwner = keyDetail.type === 'owner';

  return (
    <Box sx={{ pb: 4 }}>
      {/* 顶部导航 */}
      <Box sx={{ p: 2, display: 'flex', alignItems: 'center', gap: 2 }}>
        <IconButton onClick={() => navigate('/keys')}>
          <ArrowBack />
        </IconButton>
        <Typography variant="h5" fontWeight="bold">
          钥匙详情
        </Typography>
      </Box>

      <Box sx={{ px: 3 }}>
        <Grid container spacing={3}>
          {/* 左侧信息卡片 */}
          <Grid item xs={12} md={4}>
            <motion.div
              initial={{ opacity: 0, y: 20 }}
              animate={{ opacity: 1, y: 0 }}
            >
              <Paper elevation={2} sx={{ p: 3, borderRadius: 3 }}>
                <Box sx={{ textAlign: 'center', mb: 3 }}>
                  <Box
                    sx={{
                      width: 80,
                      height: 80,
                      borderRadius: '50%',
                      bgcolor: `${protocolColors[keyDetail.protocol]}20`,
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      mx: 'auto',
                      mb: 2,
                    }}
                  >
                    <DirectionsCar
                      sx={{ fontSize: 40, color: protocolColors[keyDetail.protocol] }}
                    />
                  </Box>
                  <Typography variant="h6" fontWeight="bold">
                    {keyDetail.vehicle.brand} {keyDetail.vehicle.model}
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    {keyDetail.vehicle.plate_number || keyDetail.vehicle.vin}
                  </Typography>
                </Box>

                <Divider sx={{ my: 2 }} />

                <Box sx={{ mb: 2 }}>
                  <Typography variant="caption" color="text.secondary">
                    协议类型
                  </Typography>
                  <Chip
                    label={keyDetail.protocol}
                    size="small"
                    sx={{
                      ml: 1,
                      bgcolor: protocolColors[keyDetail.protocol],
                      color: 'white',
                      fontWeight: 'bold',
                    }}
                  />
                </Box>

                <Box sx={{ mb: 2 }}>
                  <Typography variant="caption" color="text.secondary">
                    钥匙状态
                  </Typography>
                  <Chip
                    label={
                      keyDetail.status === 'active' ? '有效' :
                      keyDetail.status === 'inactive' ? '已停用' :
                      keyDetail.status === 'expired' ? '已过期' : '已撤销'
                    }
                    size="small"
                    color={
                      keyDetail.status === 'active' ? 'success' :
                      keyDetail.status === 'inactive' ? 'default' :
                      keyDetail.status === 'expired' ? 'warning' : 'error'
                    }
                    sx={{ ml: 1 }}
                  />
                </Box>

                <Box sx={{ mb: 2 }}>
                  <Typography variant="caption" color="text.secondary">
                    发行时间
                  </Typography>
                  <Typography variant="body2">
                    {format(new Date(keyDetail.issued_at), 'yyyy年MM月dd日 HH:mm', { locale: zhCN })}
                  </Typography>
                </Box>

                {keyDetail.expires_at && (
                  <Box sx={{ mb: 2 }}>
                    <Typography variant="caption" color="text.secondary">
                      过期时间
                    </Typography>
                    <Typography variant="body2" color={isExpired ? 'error' : 'text.primary'}>
                      {format(new Date(keyDetail.expires_at), 'yyyy年MM月dd日 HH:mm', { locale: zhCN })}
                    </Typography>
                  </Box>
                )}

                <Box sx={{ mb: 2 }}>
                  <Typography variant="caption" color="text.secondary">
                    使用次数
                  </Typography>
                  <Typography variant="body2">
                    {keyDetail.use_count} 次
                  </Typography>
                </Box>

                {keyDetail.last_used_at && (
                  <Box sx={{ mb: 2 }}>
                    <Typography variant="caption" color="text.secondary">
                      最后使用
                    </Typography>
                    <Typography variant="body2">
                      {format(new Date(keyDetail.last_used_at), 'yyyy年MM月dd日 HH:mm', { locale: zhCN })}
                    </Typography>
                  </Box>
                )}

                {keyDetail.shared_by && (
                  <Box sx={{ mb: 2 }}>
                    <Typography variant="caption" color="text.secondary">
                      分享自
                    </Typography>
                    <Typography variant="body2">
                      {keyDetail.shared_by}
                    </Typography>
                  </Box>
                )}

                <Divider sx={{ my: 2 }} />

                {/* 操作按钮 */}
                <Box sx={{ display: 'flex', flexDirection: 'column', gap: 1 }}>
                  {isOwner && isActive && (
                    <Button
                      variant="outlined"
                      startIcon={<Share />}
                      onClick={() => setShowShareDialog(true)}
                      fullWidth
                    >
                      分享钥匙
                    </Button>
                  )}
                  
                  <Button
                    variant="outlined"
                    startIcon={<QrCode />}
                    fullWidth
                  >
                    显示二维码
                  </Button>

                  {isActive ? (
                    <Button
                      variant="outlined"
                      color="warning"
                      startIcon={<Pause />}
                      onClick={() => setShowDeactivateDialog(true)}
                      fullWidth
                    >
                      停用钥匙
                    </Button>
                  ) : (
                    <Button
                      variant="outlined"
                      color="success"
                      startIcon={<Play />}
                      onClick={() => activateMutation.mutate()}
                      fullWidth
                      disabled={isExpired || keyDetail.status === 'revoked'}
                    >
                      激活钥匙
                    </Button>
                  )}

                  {isOwner && (
                    <Button
                      variant="outlined"
                      color="error"
                      startIcon={<Delete />}
                      onClick={() => revokeMutation.mutate()}
                      fullWidth
                    >
                      撤销钥匙
                    </Button>
                  )}
                </Box>
              </Paper>
            </motion.div>
          </Grid>

          {/* 右侧详情 */}
          <Grid item xs={12} md={8}>
            <Paper elevation={2} sx={{ borderRadius: 3 }}>
              <Tabs
                value={activeTab}
                onChange={(_, v) => setActiveTab(v)}
                sx={{ borderBottom: 1, borderColor: 'divider' }}
              >
                <Tab label="权限设置" icon={<Settings />} iconPosition="start" />
                <Tab label="使用记录" icon={<History />} iconPosition="start" />
                {isOwner && <Tab label="分享管理" icon={<Share />} iconPosition="start" />}
              </Tabs>

              {/* 权限设置 */}
              {activeTab === 0 && (
                <Box sx={{ p: 3 }}>
                  <Typography variant="h6" gutterBottom>
                    当前权限
                  </Typography>
                  <List>
                    {keyDetail.permissions.map((perm) => (
                      <ListItem key={perm.type}>
                        <ListItemIcon>
                          {perm.enabled ? (
                            <CheckCircle color="success" />
                          ) : (
                            <ErrorIcon color="disabled" />
                          )}
                        </ListItemIcon>
                        <ListItemText
                          primary={permissionLabels[perm.type] || perm.type}
                          secondary={perm.constraints ? '有限制条件' : '无限制'}
                        />
                        {perm.enabled && perm.constraints && (
                          <Chip
                            size="small"
                            label="限制"
                            color="info"
                            variant="outlined"
                          />
                        )}
                      </ListItem>
                    ))}
                  </List>

                  {isOwner && (
                    <>
                      <Divider sx={{ my: 2 }} />
                      <Box sx={{ display: 'flex', justifyContent: 'flex-end' }}>
                        <Button
                          variant="contained"
                          startIcon={<Edit />}
                          onClick={() => navigate(`/keys/${keyId}/edit`)}
                        >
                          编辑权限
                        </Button>
                      </Box>
                    </>
                  )}
                </Box>
              )}

              {/* 使用记录 */}
              {activeTab === 1 && (
                <Box sx={{ p: 3 }}>
                  <Typography variant="h6" gutterBottom>
                    最近50次使用
                  </Typography>
                  {usageLogs?.logs.length === 0 ? (
                    <Typography color="text.secondary" align="center" py={4}>
                      暂无使用记录
                    </Typography>
                  ) : (
                    <List>
                      {usageLogs?.logs.map((log) => (
                        <ListItem key={log.id} divider>
                          <ListItemIcon>
                            {log.status === 'success' ? (
                              <CheckCircle color="success" />
                            ) : log.status === 'failure' ? (
                              <ErrorIcon color="error" />
                            ) : (
                              <Warning color="warning" />
                            )}
                          </ListItemIcon>
                          <ListItemText
                            primary={permissionLabels[log.operation] || log.operation}
                            secondary={
                              <>
                                {format(new Date(log.timestamp), 'yyyy-MM-dd HH:mm:ss')}
                                {log.location && ` · ${log.location.lat.toFixed(4)}, ${log.location.lng.toFixed(4)}`}
                                {log.failure_reason && ` · ${log.failure_reason}`}
                              </>
                            }
                          />
                          <Chip
                            size="small"
                            label={
                              log.status === 'success' ? '成功' :
                              log.status === 'failure' ? '失败' : '超时'
                            }
                            color={
                              log.status === 'success' ? 'success' :
                              log.status === 'failure' ? 'error' : 'warning'
                            }
                          />
                        </ListItem>
                      ))}
                    </List>
                  )}
                </Box>
              )}

              {/* 分享管理 */}
              {activeTab === 2 && isOwner && (
                <Box sx={{ p: 3 }}>
                  <Typography variant="h6" gutterBottom>
                    分享管理
                  </Typography>
                  <Typography color="text.secondary">
                    已分享 {keyDetail.share_count} 次
                  </Typography>
                </Box>
              )}
            </Paper>
          </Grid>
        </Grid>
      </Box>

      {/* 停用对话框 */}
      <Dialog open={showDeactivateDialog} onClose={() => setShowDeactivateDialog(false)}>
        <DialogTitle>确认停用钥匙</DialogTitle>
        <DialogContent>
          <Typography>
            停用后，该钥匙将暂时无法使用。您可以随时重新激活。
          </Typography>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setShowDeactivateDialog(false)}>取消</Button>
          <Button
            onClick={() => deactivateMutation.mutate()}
            color="warning"
            variant="contained"
          >
            确认停用
          </Button>
        </DialogActions>
      </Dialog>
    </Box>
  );
}
