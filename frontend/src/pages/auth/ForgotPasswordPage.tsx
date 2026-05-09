import React, { useState } from 'react';
import {
  Box,
  Card,
  CardContent,
  TextField,
  Button,
  Typography,
  Link,
  Stepper,
  Step,
  StepLabel,
  InputAdornment,
  IconButton,
  Container,
  useTheme,
  useMediaQuery,
  Alert,
} from '@mui/material';
import {
  Visibility,
  VisibilityOff,
  ArrowBack,
  ArrowForward,
  CheckCircle,
  LockReset,
} from '@mui/icons-material';
import { motion, AnimatePresence } from 'framer-motion';
import { Link as RouterLink } from 'react-router-dom';

const steps = ['验证身份', '设置新密码', '完成重置'];

const ForgotPasswordPage: React.FC = () => {
  const theme = useTheme();
  const isMobile = useMediaQuery(theme.breakpoints.down('sm'));

  const [activeStep, setActiveStep] = useState(0);
  const [formData, setFormData] = useState({
    account: '',
    verificationCode: '',
    newPassword: '',
    confirmPassword: '',
  });
  const [showPassword, setShowPassword] = useState(false);
  const [showConfirmPassword, setShowConfirmPassword] = useState(false);
  const [errors, setErrors] = useState<Record<string, string>>({});
  const [countdown, setCountdown] = useState(0);
  const [success, setSuccess] = useState(false);

  const handleChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { name, value } = e.target;
    setFormData((prev) => ({
      ...prev,
      [name]: value,
    }));
    if (errors[name]) {
      setErrors((prev) => ({ ...prev, [name]: '' }));
    }
  };

  const sendVerificationCode = () => {
    if (!formData.account.trim()) {
      setErrors((prev) => ({ ...prev, account: '请输入手机号或邮箱' }));
      return;
    }
    // TODO: Implement send verification code API
    setCountdown(60);
    const timer = setInterval(() => {
      setCountdown((prev) => {
        if (prev <= 1) {
          clearInterval(timer);
          return 0;
        }
        return prev - 1;
      });
    }, 1000);
  };

  const validateStep = () => {
    const newErrors: Record<string, string> = {};

    if (activeStep === 0) {
      if (!formData.account.trim()) {
        newErrors.account = '请输入手机号或邮箱';
      }
      if (!formData.verificationCode.trim()) {
        newErrors.verificationCode = '请输入验证码';
      }
    }

    if (activeStep === 1) {
      if (!formData.newPassword) {
        newErrors.newPassword = '请输入新密码';
      } else if (formData.newPassword.length < 6) {
        newErrors.newPassword = '密码长度至少6位';
      }
      if (formData.newPassword !== formData.confirmPassword) {
        newErrors.confirmPassword = '两次输入的密码不一致';
      }
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const handleNext = () => {
    if (validateStep()) {
      if (activeStep === steps.length - 1) {
        // TODO: Implement reset password API
        setSuccess(true);
      } else {
        setActiveStep((prev) => prev + 1);
      }
    }
  };

  const handleBack = () => {
    setActiveStep((prev) => prev - 1);
  };

  const containerVariants = {
    hidden: { opacity: 0, x: 20 },
    visible: { opacity: 1, x: 0 },
    exit: { opacity: 0, x: -20 },
  };

  if (success) {
    return (
      <Container maxWidth="sm" sx={{ minHeight: '100vh', display: 'flex', alignItems: 'center' }}>
        <motion.div
          initial={{ opacity: 0, scale: 0.8 }}
          animate={{ opacity: 1, scale: 1 }}
          transition={{ duration: 0.5 }}
          style={{ width: '100%' }}
        >
          <Card sx={{ borderRadius: 3, p: 4, textAlign: 'center' }}>
            <motion.div
              initial={{ scale: 0 }}
              animate={{ scale: 1 }}
              transition={{ delay: 0.2, type: 'spring', stiffness: 200 }}
            >
              <CheckCircle sx={{ fontSize: 80, color: 'success.main', mb: 2 }} />
            </motion.div>
            <Typography variant="h4" gutterBottom sx={{ fontWeight: 700 }}>
              密码重置成功！
            </Typography>
            <Typography variant="body1" color="text.secondary" sx={{ mb: 4 }}>
              您的密码已重置，请使用新密码登录
            </Typography>
            <Button
              component={RouterLink}
              to="/auth/login"
              variant="contained"
              size="large"
              sx={{ borderRadius: 2, px: 4 }}
            >
              去登录
            </Button>
          </Card>
        </motion.div>
      </Container>
    );
  }

  return (
    <Container maxWidth="sm" sx={{ minHeight: '100vh', display: 'flex', alignItems: 'center' }}>
      <motion.div
        initial={{ opacity: 0, y: 20 }}
        animate={{ opacity: 1, y: 0 }}
        transition={{ duration: 0.5 }}
        style={{ width: '100%' }}
      >
        <Card
          elevation={isMobile ? 0 : 4}
          sx={{
            borderRadius: isMobile ? 0 : 3,
            background: isMobile ? 'transparent' : 'rgba(255, 255, 255, 0.95)',
            backdropFilter: 'blur(10px)',
          }}
        >
          <CardContent sx={{ p: isMobile ? 2 : 4 }}>
            <Box sx={{ textAlign: 'center', mb: 1 }}>
              <LockReset
                sx={{
                  fontSize: 48,
                  color: 'primary.main',
                  mb: 1,
                }}
              />
            </Box>
            <Typography
              variant="h4"
              component="h1"
              align="center"
              sx={{
                fontWeight: 700,
                mb: 1,
                background: `linear-gradient(135deg, ${theme.palette.primary.main}, ${theme.palette.secondary.main})`,
                WebkitBackgroundClip: 'text',
                WebkitTextFillColor: 'transparent',
              }}
            >
              忘记密码
            </Typography>
            <Typography variant="body2" color="text.secondary" align="center" sx={{ mb: 3 }}>
              验证身份后重置您的密码
            </Typography>

            <Stepper activeStep={activeStep} sx={{ mb: 4 }} alternativeLabel>
              {steps.map((label) => (
                <Step key={label}>
                  <StepLabel>{isMobile ? '' : label}</StepLabel>
                </Step>
              ))}
            </Stepper>

            <AnimatePresence mode="wait">
              <motion.div
                key={activeStep}
                variants={containerVariants}
                initial="hidden"
                animate="visible"
                exit="exit"
                transition={{ duration: 0.3 }}
              >
                {activeStep === 0 && (
                  <Box>
                    <Alert severity="info" sx={{ mb: 3, borderRadius: 2 }}>
                      <Typography variant="body2">
                        我们将向您的手机或邮箱发送验证码
                      </Typography>
                    </Alert>
                    <TextField
                      fullWidth
                      label="手机号/邮箱"
                      name="account"
                      value={formData.account}
                      onChange={handleChange}
                      error={!!errors.account}
                      helperText={errors.account}
                      margin="normal"
                      autoComplete="email"
                      autoFocus
                      sx={{ '& .MuiOutlinedInput-root': { borderRadius: 2 } }}
                    />
                    <TextField
                      fullWidth
                      label="验证码"
                      name="verificationCode"
                      value={formData.verificationCode}
                      onChange={handleChange}
                      error={!!errors.verificationCode}
                      helperText={errors.verificationCode}
                      margin="normal"
                      InputProps={{
                        endAdornment: (
                          <InputAdornment position="end">
                            <Button
                              size="small"
                              onClick={sendVerificationCode}
                              disabled={countdown > 0}
                              sx={{ minWidth: 100 }}
                            >
                              {countdown > 0 ? `${countdown}s` : '获取验证码'}
                            </Button>
                          </InputAdornment>
                        ),
                      }}
                      sx={{ '& .MuiOutlinedInput-root': { borderRadius: 2 } }}
                    />
                  </Box>
                )}

                {activeStep === 1 && (
                  <Box>
                    <Alert severity="warning" sx={{ mb: 3, borderRadius: 2 }}>
                      <Typography variant="body2">
                        请设置一个安全的新密码
                      </Typography>
                    </Alert>
                    <TextField
                      fullWidth
                      label="新密码"
                      name="newPassword"
                      type={showPassword ? 'text' : 'password'}
                      value={formData.newPassword}
                      onChange={handleChange}
                      error={!!errors.newPassword}
                      helperText={errors.newPassword || '密码长度至少6位'}
                      margin="normal"
                      autoFocus
                      InputProps={{
                        endAdornment: (
                          <InputAdornment position="end">
                            <IconButton
                              onClick={() => setShowPassword(!showPassword)}
                              edge="end"
                              size="small"
                            >
                              {showPassword ? <VisibilityOff /> : <Visibility />}
                            </IconButton>
                          </InputAdornment>
                        ),
                      }}
                      sx={{ '& .MuiOutlinedInput-root': { borderRadius: 2 } }}
                    />
                    <TextField
                      fullWidth
                      label="确认新密码"
                      name="confirmPassword"
                      type={showConfirmPassword ? 'text' : 'password'}
                      value={formData.confirmPassword}
                      onChange={handleChange}
                      error={!!errors.confirmPassword}
                      helperText={errors.confirmPassword}
                      margin="normal"
                      InputProps={{
                        endAdornment: (
                          <InputAdornment position="end">
                            <IconButton
                              onClick={() => setShowConfirmPassword(!showConfirmPassword)}
                              edge="end"
                              size="small"
                            >
                              {showConfirmPassword ? <VisibilityOff /> : <Visibility />}
                            </IconButton>
                          </InputAdornment>
                        ),
                      }}
                      sx={{ '& .MuiOutlinedInput-root': { borderRadius: 2 } }}
                    />
                  </Box>
                )}

                {activeStep === 2 && (
                  <Box sx={{ textAlign: 'center', py: 2 }}>
                    <Typography variant="h6" gutterBottom>
                      确认重置密码？
                    </Typography>
                    <Typography variant="body2" color="text.secondary">
                      点击"确认重置"将使用新密码替换您的旧密码
                    </Typography>
                  </Box>
                )}
              </motion.div>
            </AnimatePresence>

            <Box sx={{ display: 'flex', justifyContent: 'space-between', mt: 4, gap: 2 }}>
              <Button
                onClick={handleBack}
                disabled={activeStep === 0}
                startIcon={<ArrowBack />}
                sx={{ borderRadius: 2 }}
              >
                上一步
              </Button>
              <Button
                onClick={handleNext}
                variant="contained"
                endIcon={activeStep === steps.length - 1 ? undefined : <ArrowForward />}
                sx={{
                  borderRadius: 2,
                  px: 4,
                  boxShadow: `0 8px 16px ${theme.palette.primary.main}40`,
                }}
              >
                {activeStep === steps.length - 1 ? '确认重置' : '下一步'}
              </Button>
            </Box>

            <Typography variant="body2" align="center" sx={{ mt: 3 }}>
              记起密码了？{' '}
              <Link
                component={RouterLink}
                to="/auth/login"
                color="primary"
                sx={{ fontWeight: 600, textDecoration: 'none' }}
              >
                返回登录
              </Link>
            </Typography>
          </CardContent>
        </Card>
      </motion.div>
    </Container>
  );
};

export default ForgotPasswordPage;
