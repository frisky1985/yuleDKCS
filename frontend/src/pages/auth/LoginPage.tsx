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
  Divider,
  IconButton,
  InputAdornment,
  Container,
  useTheme,
  useMediaQuery,
} from '@mui/material';
import {
  Visibility,
  VisibilityOff,
  Google,
  Apple,
  Wechat,
} from '@mui/icons-material';
import { motion } from 'framer-motion';
import { Link as RouterLink, useNavigate } from 'react-router-dom';

const LoginPage: React.FC = () => {
  const theme = useTheme();
  const isMobile = useMediaQuery(theme.breakpoints.down('sm'));
  const navigate = useNavigate();

  const [formData, setFormData] = useState({
    account: '',
    password: '',
    rememberMe: false,
  });
  const [showPassword, setShowPassword] = useState(false);
  const [errors, setErrors] = useState<Record<string, string>>({});

  const handleChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const { name, value, checked, type } = e.target;
    setFormData((prev) => ({
      ...prev,
      [name]: type === 'checkbox' ? checked : value,
    }));
    // Clear error when user types
    if (errors[name]) {
      setErrors((prev) => ({ ...prev, [name]: '' }));
    }
  };

  const validateForm = () => {
    const newErrors: Record<string, string> = {};
    if (!formData.account.trim()) {
      newErrors.account = '请输入手机号或邮箱';
    }
    if (!formData.password) {
      newErrors.password = '请输入密码';
    } else if (formData.password.length < 6) {
      newErrors.password = '密码长度至少6位';
    }
    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (validateForm()) {
      // TODO: Implement login API call
      console.log('Login:', formData);
    }
  };

  const handleSocialLogin = (provider: string) => {
    // TODO: Implement social login
    console.log('Social login:', provider);
  };

  const containerVariants = {
    hidden: { opacity: 0, y: 20 },
    visible: {
      opacity: 1,
      y: 0,
      transition: {
        duration: 0.5,
        staggerChildren: 0.1,
      },
    },
  };

  const itemVariants = {
    hidden: { opacity: 0, y: 10 },
    visible: { opacity: 1, y: 0 },
  };

  return (
    <Container maxWidth="sm" sx={{ minHeight: '100vh', display: 'flex', alignItems: 'center' }}>
      <motion.div
        variants={containerVariants}
        initial="hidden"
        animate="visible"
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
            <motion.div variants={itemVariants}>
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
                欢迎回来
              </Typography>
              <Typography variant="body2" color="text.secondary" align="center" sx={{ mb: 4 }}>
                登录您的账户，开始游戏之旅
              </Typography>
            </motion.div>

            <Box component="form" onSubmit={handleSubmit} noValidate>
              <motion.div variants={itemVariants}>
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
                  sx={{
                    '& .MuiOutlinedInput-root': {
                      borderRadius: 2,
                    },
                  }}
                />
              </motion.div>

              <motion.div variants={itemVariants}>
                <TextField
                  fullWidth
                  label="密码"
                  name="password"
                  type={showPassword ? 'text' : 'password'}
                  value={formData.password}
                  onChange={handleChange}
                  error={!!errors.password}
                  helperText={errors.password}
                  margin="normal"
                  autoComplete="current-password"
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
                  sx={{
                    '& .MuiOutlinedInput-root': {
                      borderRadius: 2,
                    },
                  }}
                />
              </motion.div>

              <motion.div variants={itemVariants}>
                <Box
                  sx={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    mt: 1,
                    mb: 2,
                  }}
                >
                  <FormControlLabel
                    control={
                      <Checkbox
                        name="rememberMe"
                        checked={formData.rememberMe}
                        onChange={handleChange}
                        color="primary"
                        size="small"
                      />
                    }
                    label={<Typography variant="body2">记住我</Typography>}
                  />
                  <Link
                    component={RouterLink}
                    to="/auth/forgot-password"
                    variant="body2"
                    color="primary"
                    sx={{ textDecoration: 'none' }}
                  >
                    忘记密码？
                  </Link>
                </Box>
              </motion.div>

              <motion.div variants={itemVariants}>
                <Button
                  type="submit"
                  fullWidth
                  variant="contained"
                  size="large"
                  sx={{
                    py: 1.5,
                    borderRadius: 2,
                    fontSize: '1rem',
                    fontWeight: 600,
                    textTransform: 'none',
                    boxShadow: `0 8px 16px ${theme.palette.primary.main}40`,
                    '&:hover': {
                      boxShadow: `0 12px 24px ${theme.palette.primary.main}60`,
                    },
                  }}
                >
                  登录
                </Button>
              </motion.div>
            </Box>

            <motion.div variants={itemVariants}>
              <Box sx={{ my: 3 }}>
                <Divider>
                  <Typography variant="body2" color="text.secondary" sx={{ px: 2 }}>
                    或使用以下方式
                  </Typography>
                </Divider>
              </Box>
            </motion.div>

            <motion.div variants={itemVariants}>
              <Box sx={{ display: 'flex', gap: 2, justifyContent: 'center', mb: 3 }}>
                <IconButton
                  onClick={() => handleSocialLogin('google')}
                  sx={{
                    width: 48,
                    height: 48,
                    border: `1px solid ${theme.palette.divider}`,
                    borderRadius: 2,
                    '&:hover': {
                      backgroundColor: theme.palette.action.hover,
                    },
                  }}
                >
                  <Google sx={{ color: '#DB4437' }} />
                </IconButton>
                <IconButton
                  onClick={() => handleSocialLogin('apple')}
                  sx={{
                    width: 48,
                    height: 48,
                    border: `1px solid ${theme.palette.divider}`,
                    borderRadius: 2,
                    '&:hover': {
                      backgroundColor: theme.palette.action.hover,
                    },
                  }}
                >
                  <Apple />
                </IconButton>
                <IconButton
                  onClick={() => handleSocialLogin('wechat')}
                  sx={{
                    width: 48,
                    height: 48,
                    border: `1px solid ${theme.palette.divider}`,
                    borderRadius: 2,
                    '&:hover': {
                      backgroundColor: theme.palette.action.hover,
                    },
                  }}
                >
                  <Wechat sx={{ color: '#07C160' }} />
                </IconButton>
              </Box>
            </motion.div>

            <motion.div variants={itemVariants}>
              <Typography variant="body2" align="center" color="text.secondary">
                还没有账户？{' '}
                <Link
                  component={RouterLink}
                  to="/auth/register"
                  color="primary"
                  sx={{ fontWeight: 600, textDecoration: 'none' }}
                >
                  立即注册
                </Link>
              </Typography>
            </motion.div>
          </CardContent>
        </Card>
      </motion.div>
    </Container>
  );
};

export default LoginPage;
