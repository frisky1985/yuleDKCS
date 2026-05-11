import { useState } from 'react';
import {
  Dialog, DialogTitle, DialogContent, DialogActions, Button,
  TextField, Stepper, Step, StepLabel, Box, Typography,
  FormControlLabel, Checkbox, Grid, Chip, Divider, Alert,
  ToggleButtonGroup, ToggleButton, Slider
} from '@mui/material';
import { QrCode, Email, Phone, Link as LinkIcon } from '@mui/icons-material';
import { keysApi, type KeyPermission } from '../api/keys';
import { addDays } from 'date-fns';

interface ShareKeyDialogProps {
  open: boolean;
  onClose: () => void;
  keyId: string;
  vehicleName: string;
}

const allPermissions: { type: KeyPermission['type']; label: string; icon: string }[] = [
  { type: 'unlock', label: '解锁', icon: '🔓' },
  { type: 'lock', label: '锁车', icon: '🔒' },
  { type: 'start_engine', label: '启动引擎', icon: '🚗' },
  { type: 'stop_engine', label: '停止引擎', icon: '⏹️' },
  { type: 'open_trunk', label: '开后备箱', icon: '📦' },
  { type: 'open_windows', label: '开窗', icon: '🐠' },
  { type: 'close_windows', label: '关窗', icon: '🐡' },
  { type: 'find_vehicle', label: '寻车', icon: '📅' },
];

const expiryOptions = [
  { days: 1, label: '1天' },
  { days: 3, label: '3天' },
  { days: 7, label: '7天' },
  { days: 30, label: '30天' },
  { days: 365, label: '1年' },
];

export default function ShareKeyDialog({ open, onClose, keyId, vehicleName }: ShareKeyDialogProps) {
  const [activeStep, setActiveStep] = useState(0);
  const [shareMethod, setShareMethod] = useState<'email' | 'phone' | 'qrcode'>('email');
  const [recipient, setRecipient] = useState('');
  const [selectedPermissions, setSelectedPermissions] = useState<string[]>(['unlock', 'lock']);
  const [expiryDays, setExpiryDays] = useState(7);
  const [maxUses, setMaxUses] = useState<number | null>(null);
  const [timeRange, setTimeRange] = useState<{ start: string; end: string } | null>(null);
  const [shareResult, setShareResult] = useState<{ qrCodeUrl: string; shareLink: string } | null>(null);
  const [loading, setLoading] = useState(false);

  const steps = ['选择方式', '设置权限', '确认分享'];

  const handlePermissionToggle = (permType: string) => {
    setSelectedPermissions(prev =>
      prev.includes(permType)
        ? prev.filter(p => p !== permType)
        : [...prev, permType]
    );
  };

  const handleShare = async () => {
    setLoading(true);
    try {
      const permissions: KeyPermission[] = allPermissions.map(p => ({
        type: p.type,
        enabled: selectedPermissions.includes(p.type),
        constraints: p.type === 'unlock' && maxUses ? { max_uses: maxUses } : undefined,
      }));

      const result = await keysApi.shareKey({
        key_id: keyId,
        recipient_email: shareMethod === 'email' ? recipient : undefined,
        recipient_phone: shareMethod === 'phone' ? recipient : undefined,
        expires_in_days: expiryDays,
        permissions,
      });

      setShareResult({
        qrCodeUrl: result.qr_code_url,
        shareLink: result.share_link,
      });
      setActiveStep(3); // 完成步骤
    } catch (error) {
      console.error('Share failed:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleClose = () => {
    setActiveStep(0);
    setRecipient('');
    setSelectedPermissions(['unlock', 'lock']);
    setShareResult(null);
    onClose();
  };

  const renderStepContent = () => {
    switch (activeStep) {
      case 0:
        return (
          <Box sx={{ py: 2 }}>
            <Typography variant="subtitle1" gutterBottom>
              选择分享方式
            </Typography>
            <ToggleButtonGroup
              value={shareMethod}
              exclusive
              onChange={(_, value) => value && setShareMethod(value)}
              fullWidth
              sx={{ mb: 3 }}
            >
              <ToggleButton value="email">
                <Email sx={{ mr: 1 }} /> 邮件
              </ToggleButton>
              <ToggleButton value="phone">
                <Phone sx={{ mr: 1 }} /> 手机号
              </ToggleButton>
              <ToggleButton value="qrcode">
                <QrCode sx={{ mr: 1 }} /> 二维码
              </ToggleButton>
            </ToggleButtonGroup>

            {shareMethod !== 'qrcode' && (
              <TextField
                fullWidth
                label={shareMethod === 'email' ? '接收邮箱' : '接收手机号'}
                value={recipient}
                onChange={(e) => setRecipient(e.target.value)}
                placeholder={shareMethod === 'email' ? 'example@email.com' : '138xxxxxxxx'}
              />
            )}

            <Box sx={{ mt: 3 }}>
              <Typography variant="subtitle2" gutterBottom>
                有效期设置
              </Typography>
              <ToggleButtonGroup
                value={expiryDays}
                exclusive
                onChange={(_, value) => value && setExpiryDays(value)}
                size="small"
              >
                {expiryOptions.map(opt => (
                  <ToggleButton key={opt.days} value={opt.days}>
                    {opt.label}
                  </ToggleButton>
                ))}
              </ToggleButtonGroup>
            </Box>
          </Box>
        );

      case 1:
        return (
          <Box sx={{ py: 2 }}>
            <Typography variant="subtitle1" gutterBottom>
              选择授予的权限
            </Typography>
            <Alert severity="info" sx={{ mb: 2 }}>
              车主钥匙可分享所有权限，普通钥匙只能分享自身拥有的权限
            </Alert>
            
            <Grid container spacing={2}>
              {allPermissions.map((perm) => (
                <Grid item xs={6} key={perm.type}>
                  <FormControlLabel
                    control={
                      <Checkbox
                        checked={selectedPermissions.includes(perm.type)}
                        onChange={() => handlePermissionToggle(perm.type)}
                      />
                    }
                    label={
                      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                        <span>{perm.icon}</span>
                        <span>{perm.label}</span>
                      </Box>
                    }
                  />
                </Grid>
              ))}
            </Grid>

            <Divider sx={{ my: 2 }} />

            <Typography variant="subtitle2" gutterBottom>
              高级限制（可选）
            </Typography>
            
            <Box sx={{ mb: 2 }}>
              <Typography variant="body2" color="text.secondary" gutterBottom>
                最大使用次数: {maxUses || '无限制'}
              </Typography>
              <Slider
                value={maxUses || 100}
                onChange={(_, value) => setMaxUses(value === 100 ? null : (value as number))}
                min={1}
                max={100}
                marks={[{ value: 100, label: '无限' }]}
              />
            </Box>
          </Box>
        );

      case 2:
        return (
          <Box sx={{ py: 2 }}>
            <Typography variant="h6" gutterBottom>
              分享确认
            </Typography>
            
            <Paper variant="outlined" sx={{ p: 2, mb: 2 }}>
              <Typography variant="subtitle2" color="text.secondary">
                车辆
              </Typography>
              <Typography variant="body1" gutterBottom>
                {vehicleName}
              </Typography>

              <Typography variant="subtitle2" color="text.secondary">
                接收方
              </Typography>
              <Typography variant="body1" gutterBottom>
                {shareMethod === 'qrcode' ? '通过二维码扫描' : recipient}
              </Typography>

              <Typography variant="subtitle2" color="text.secondary">
                有效期
              </Typography>
              <Typography variant="body1" gutterBottom>
                {expiryDays} 天
              </Typography>

              <Typography variant="subtitle2" color="text.secondary">
                授予权限
              </Typography>
              <Box sx={{ display: 'flex', flexWrap: 'wrap', gap: 0.5, mt: 0.5 }}>
                {selectedPermissions.map(perm => (
                  <Chip
                    key={perm}
                    size="small"
                    label={allPermissions.find(p => p.type === perm)?.label}
                  />
                ))}
              </Box>
            </Paper>
          </Box>
        );

      case 3:
        return (
          <Box sx={{ py: 2, textAlign: 'center' }}>
            <Typography variant="h6" color="success.main" gutterBottom>
              分享成功！
            </Typography>
            
            {shareMethod === 'qrcode' && shareResult && (
              <Box sx={{ my: 2 }}>
                <img
                  src={shareResult.qrCodeUrl}
                  alt="Share QR Code"
                  style={{ width: 200, height: 200 }}
                />
                <Typography variant="caption" display="block" sx={{ mt: 1 }}>
                  让对方扫描此二维码接收钥匙
                </Typography>
              </Box>
            )}

            {shareResult && (
              <Box sx={{ mt: 2 }}>
                <Typography variant="body2" color="text.secondary">
                  或者通过链接分享：
                </Typography>
                <Box
                  sx={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: 1,
                    mt: 1,
                  }}
                >
                  <Typography
                    variant="body2"
                    sx={{
                      bgcolor: 'grey.100',
                      px: 2,
                      py: 0.5,
                      borderRadius: 1,
                      maxWidth: 300,
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                    }}
                  >
                    {shareResult.shareLink}
                  </Typography>
                  <Button
                    size="small"
                    onClick={() => navigator.clipboard.writeText(shareResult.shareLink)}
                  >
                    复制
                  </Button>
                </Box>
              </Box>
            )}
          </Box>
        );
    }
  };

  return (
    <Dialog open={open} onClose={handleClose} maxWidth="sm" fullWidth>
      <DialogTitle>分享钥匙 - {vehicleName}
      </DialogTitle>
      <DialogContent>
        {activeStep < 3 && (
          <Stepper activeStep={activeStep} sx={{ mb: 2 }}>
            {steps.map(label => <Step key={label}><StepLabel>{label}</StepLabel></Step>)}
          </Stepper>
        )}
        {renderStepContent()}
      </DialogContent>
      <DialogActions>
        {activeStep === 0 ? (
          <Button onClick={handleClose}>取消</Button>
        ) : activeStep < 3 ? (
          <Button onClick={() => setActiveStep(prev => prev - 1)}>上一步</Button>
        ) : null}
        
        {activeStep < 2 && (
          <Button
            variant="contained"
            onClick={() => setActiveStep(prev => prev + 1)}
            disabled={activeStep === 0 && shareMethod !== 'qrcode' && !recipient}
          >
            下一步
          </Button>
        )}
        
        {activeStep === 2 && (
          <Button
            variant="contained"
            onClick={handleShare}
            disabled={loading || selectedPermissions.length === 0}
          >
            {loading ? '分享中...' : '确认分享'}
          </Button>
        )}
        
        {activeStep === 3 && (
          <Button variant="contained" onClick={handleClose}>
            完成
          </Button>
        )}
      </DialogActions>
    </Dialog>
  );
}
