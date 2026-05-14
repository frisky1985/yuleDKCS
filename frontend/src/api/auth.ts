import apiClient from './client';

export interface LoginRequest {
  username: string;
  password: string;
}

export interface RegisterRequest {
  username: string;
  email: string;
  password: string;
  phone?: string;
}

export interface LoginResponse {
  code: number;
  message: string;
  data: {
    token: string;
    type: string;
    user: {
      id: string;
      username: string;
      email: string;
      role: string;
    };
  };
}

export interface UserProfile {
  code: number;
  data: {
    id: string;
    username: string;
    email: string;
    phone?: string;
    role: string;
    created_at: string;
  };
}

export const authApi = {
  // 登录
  login: (data: LoginRequest) =>
    apiClient.post<LoginResponse>('/auth/login', data).then(r => r.data),

  // 注册
  register: (data: RegisterRequest) =>
    apiClient.post('/auth/register', data).then(r => r.data),

  // 刷新Token
  refreshToken: () =>
    apiClient.post('/auth/refresh').then(r => r.data),

  // 获取用户信息
  getProfile: () =>
    apiClient.get<UserProfile>('/user/profile').then(r => r.data),

  // 登出（可选，用于后端记录）
  logout: () =>
    apiClient.post('/auth/logout').then(r => r.data),
};

export default authApi;
