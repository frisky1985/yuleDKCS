import { useState } from 'react';
import {
  Box, Typography, Tabs, Tab, Card, CardContent, Chip,
  Avatar, Button, IconButton, Dialog, DialogTitle, DialogContent,
  DialogActions, TextField, Select, MenuItem, FormControl,
  InputLabel, List, ListItem, ListItemText, ListItemSecondaryAction,
  FormControlLabel, Checkbox, Divider
} from '@mui/material';
import {
  Share, Delete, Key, ContentCopy, QrCode,
  AccessTime, CheckCircle, Cancel
} from '@mui/icons-material';
import { motion, AnimatePresence } from 'framer-motion';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { api } from '../api/client';

interface Key {
  id: number;
  vehicle: { brand: string; model: string };
  type: 'owner' | 'shared';
  protocol: 'CCC' | 'ICCE' | 'ICCOA';
  status: 'active' | 'inactive' | 'expired';
  permissions: string[];
  expires_at: string;
  shared_by?: string;
}

export default function KeysPage() {
  const [tab, setTab] = useState(0);
  const [shareOpen, setShareOpen] = useState(false);
  const [selectedKey, setSelectedKey] = useState<Key | null>(null);
  const queryClient = useQueryClient();

  const { data: myKeys } = useQuery<Key[]>({
    queryKey: ['keys', 'my'],
    queryFn: () => api.get('/keys').then(r => r.data.keys),
  });

  const { data: sharedKeys } = useQuery<Key[]>({
    queryKey: ['keys', 'shared'],
    queryFn: () => api.get('/keys/shared/list').then(r => r.data.keys),
  });

  const revokeMutation = useMutation({
    mutationFn: (keyId: number) => api.delete(`/keys/${keyId}`),
    onSuccess: () => queryClient.invalidateQueries({ queryKey: ['keys'] }),
  });

  const handleShare = (key: Key) => {
    setSelectedKey(key);
    setShareOpen(true);
  };

  const getProtocolColor = (protocol: string) => {
    switch (protocol) {
      case 'CCC': return '#2196f3';
      case 'ICCE': return '#4caf50';
      case 'ICCOA': return '#ff9800';
      default: return '#9e9e9e';
    }
  };

  const KeyCard = ({ keyItem }: { keyItem: Key }) => (
    <motion.div
      layout
      initial={{ opacity: 0, scale: 0.95 }}
      animate={{ opacity: 1, scale: 1 }}
      exit={{ opacity: 0, scale: 0.95 }}
    >
      <Card sx={{ mb: 2, borderRadius: 3 }}>
        <CardContent>
          <Box sx={{ display: 'flex', alignItems: 'center', gap: 2, mb: 2 }}>
            <Avatar sx={{ bgcolor: getProtocolColor(keyItem.protocol) }}>
              <Key />
            </Avatar>
            <Box sx={{ flex: 1 }}>
              <Typography variant="h6" fontWeight="bold">
                {keyItem.vehicle.brand} {keyItem.vehicle.model}
              </Typography>
              <Box sx={{ display: 'flex', gap: 1, mt: 0.5 }}>
                <Chip 
                  label={keyItem.protocol} 
                  size="small"
                  sx={{ 
                    bgcolor: `${getProtocolColor(keyItem.protocol)}20`,
                    color: getProtocolColor(keyItem.protocol),
                    fontWeight: 'bold'
                  }}
                />
                <Chip 
                  label={keyItem.status === 'active' ? '有效' : keyItem.status === 'expired' ? '已过期' : '已停用'}
                  size="small"
                  color={keyItem.status === 'active' ? 'success' : 'default'}
                />
              </Box>
            </Box>
          </Box>

          <Divider sx={{ my: 1.5 }} />

          {/* 权限列表 */}
          <Box sx={{ mb: 2 }}>
            <Typography variant="caption" color="text.secondary">
              权限
            </Typography>
            <Box sx={{ display: 'flex', flexWrap: 'wrap', gap: 0.5, mt: 0.5 }}>
              {keyItem.permissions.map(p => (
                <Chip key={p} label={p} size="small" variant="outlined" />
              ))}
            </Box>
          </Box>

          {/* 过期时间 */}
          {keyItem.expires_at && (
            <Box sx={{ display: 'flex', alignItems: 'center', gap: 1, mb: 2 }}>
              <AccessTime fontSize="small" color="action" />
              <Typography variant="body2" color="text.secondary">
                过期时间: {new Date(keyItem.expires_at).toLocaleDateString('zh-CN')}
              </Typography>
            </Box>
          )}

          {/* 操作按钮 */}
          <Box sx={{ display: 'flex', gap: 1 }}>
            {keyItem.type === 'owner' && (
              <Button
                variant="outlined"
                size="small"
                startIcon={<Share />}
                onClick={() => handleShare(keyItem)}
                fullWidth
              >
                分享
              </Button>
            )}
            <Button
              variant="outlined"
              size="small"
              startIcon={<QrCode />}
              fullWidth
            >
              二维码
            </Button>
            <IconButton 
              color="error" 
              size="small"
              onClick={() => revokeMutation.mutate(keyItem.id)}
            >
              <Delete />
            </IconButton>
          </Box>
        </CardContent>
      </Card>
    </motion.div>
  );

  return (
    <Box sx={{ pb: 8 }}>
      <Box sx={{ p: 2, pt: 3 }}>
        <Typography variant="h5" fontWeight="bold">
          我的钥匙
        </Typography>
      </Box>

      <Tabs 
        value={tab} 
        onChange={(_, v) => setTab(v)}
        sx={{ px: 2, borderBottom: 1, borderColor: 'divider' }}
      >
        <Tab label={`我的钥匙 (${myKeys?.length || 0})`} />
        <Tab label={`分享给我 (${sharedKeys?.length || 0})`} />
      </Tabs>

      <Box sx={{ p: 2 }}>
        <AnimatePresence mode="popLayout">
          {tab === 0 ? (
            myKeys?.map(key => <KeyCard key={key.id} keyItem={key} />)
          ) : (
            sharedKeys?.map(key => <KeyCard key={key.id} keyItem={key} />)
          )}
        </AnimatePresence>
      </Box>

      {/* 分享对话框 */}
      <Dialog open={shareOpen} onClose={() => setShareOpen(false)} maxWidth="sm" fullWidth>
        <DialogTitle>分享钥匙</DialogTitle>
        <DialogContent>
          <Typography variant="body2" color="text.secondary" sx={{ mb: 2 }}>
            分享 {selectedKey?.vehicle.brand} {selectedKey?.vehicle.model} 的钥匙
          </Typography>

          <TextField
            fullWidth
            label="手机号或邮箱"
            placeholder="输入接收者的手机号或邮箱"
            sx={{ mb: 2 }}
          />

          <FormControl fullWidth sx={{ mb: 2 }}>
            <InputLabel>有效期</InputLabel>
            <Select value={7} label="有效期">
              <MenuItem value={1}>1 天</MenuItem>
              <MenuItem value={7}>7 天</MenuItem>
              <MenuItem value={30}>30 天</MenuItem>
              <MenuItem value={365}>1 年</MenuItem>
            </Select>
          </FormControl>

          <Typography variant="subtitle2" sx={{ mb: 1 }}>
            授予权限
          </Typography>
          <Box sx={{ display: 'flex', flexDirection: 'column' }}>
            {['解锁', '锁车', '启动引擎', '开启后备箱'].map(p => (
              <FormControlLabel
                key={p}
                control={<Checkbox defaultChecked={p !== '启动引擎'} />}
                label={p}
              />
            ))}
          </Box>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setShareOpen(false)}>取消</Button>
          <Button variant="contained" onClick={() => setShareOpen(false)}>
            分享
          </Button>
        </DialogActions>
      </Dialog>
    </Box>
  );
}
