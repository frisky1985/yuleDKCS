import React, { useState } from 'react';
import {
  Box,
  Card,
  CardContent,
  TextField,
  Button,
  Typography,
  Link,
  Checkbox,
  FormControlLabel,
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
} from '@mui/icons-material';
import { motion, AnimatePresence } from 'framer-motion';
import { Link as RouterLink } from 'react-router-dom';

const steps = ['验证账户', '设置密码', '完成注册'];

const RegisterPage: React.FC = () => {
  const theme = useTheme();
  const isMobile = useMediaQuery(theme.breakpoints.down('sm'));

  const [activeStep, setActiveStep] = useState(0);
  const [formData, setFormData] = useState({
    account: '',
    verificationCode: '',
    password: '',
    confirmPassword: '',
    agreement: false,
  });
  const [showPassword, setShowPassword] = useState(false);
  const [showConfirmPassword, setShowConfirmPassword] = useState(false);
  const [errors, setErrors] = useState<Record<string, string>>({});
  const [countdown, setCountdown] = useState(0);
  const [success, setSuccess] = useState(false);

  const handleChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { name, value, checked, type } = e.target;
    setFormData((prev) => ({
      ...prev,
      [name]: type === 'checkbox' ? checked : value,
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
      if (!formData.password) {
        newErrors.password = '请设置密码';
      } else if (formData.password.length < 6) {
        newErrors.password = '密码长度至少6位';
      }
      if (formData.password !== formData.confirmPassword) {
        newErrors.confirmPassword = '两次输入的密码不一致';
      }
    }

    if (activeStep === 2 && !formData.agreement) {
      newErrors.agreement = '请同意用户协议';
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const handleNext = () => {
    if (validateStep()) {
      if (activeStep === steps.length - 1) {
        // TODO: Implement register API
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
              注册成功！
            </Typography>
            <Typography variant="body1" color="text.secondary" sx={{ mb: 4 }}>
              您的账户已创建成功，现在可以登录了
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
              创建账户
            </Typography>
            <Typography variant="body2" color="text.secondary" align="center" sx={{ mb: 3 }}>
              填写以下信息完成注册
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
                      autoFocus
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
                    <TextField
                      fullWidth
                      label="设置密码"
                      name="password"
                      type={showPassword ? 'text' : 'password'}
                      value={formData.password}
                      onChange={handleChange}
                      error={!!errors.password}
                      helperText={errors.password || '密码长度至少6位'}
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
                      label="确认密码"
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
                  <Box>
                    <Alert severity="info" sx={{ mb: 3, borderRadius: 2 }}>
                      <Typography variant="body2">
                        账户：{formData.account}
                      </Typography>
                    </Alert>
                    <FormControlLabel
                      control={
                        <Checkbox
                          name="agreement"
                          checked={formData.agreement}
                          onChange={handleChange}
                          color="primary"
                        />
                      }
                      label={
                        <Typography variant="body2">
                          我已阅读并同意{' '}
                          <Link href="#" color="primary">
                            用户协议
                          </Link>{' '}
                          和{' '}
                          <Link href="#" color="primary">
                            隐私政策
                          </Link>
                        </Typography>
                      }
                    />
                    {errors.agreement && (
                      <Typography variant="caption" color="error" sx={{ ml: 4 }}>
                        {errors.agreement}
                      </Typography>
                    )}
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
                {activeStep === steps.length - 1 ? '完成注册' : '下一步'}
              </Button>
            </Box>

            <Typography variant="body2" align="center" sx={{ mt: 3 }}>
              已有账户？{' '}
              <Link
                component={RouterLink}
                to="/auth/login"
                color="primary"
                sx={{ fontWeight: 600, textDecoration: 'none' }}
              >
                立即登录
              </Link>
            </Typography>
          </CardContent>
        </Card>
      </motion.div>
    </Container>
  );
};

export default RegisterPage;
