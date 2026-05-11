import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom'
import { useAuthStore } from '../store/auth'
import LoginPage from '../pages/Login'
import DashboardPage from '../pages/Dashboard'
import KeysPage from '../pages/KeysPage'
import KeyDetailPage from '../pages/KeyDetailPage'
import KeyUsageLogsPage from '../pages/KeyUsageLogsPage'

// 受保护路由组件
const ProtectedRoute = ({ children }: { children: React.ReactNode }) => {
  const { isAuthenticated } = useAuthStore()
  return isAuthenticated ? <>{children}</> : <Navigate to="/login" replace />
}

// 公开路由组件（已登录用户重定向到仪表板）
const PublicRoute = ({ children }: { children: React.ReactNode }) => {
  const { isAuthenticated } = useAuthStore()
  return !isAuthenticated ? <>{children}</> : <Navigate to="/" replace />
}

const AppRouter = () => {
  return (
    <BrowserRouter>
      <Routes>
        <Route
          path="/login"
          element={
            <PublicRoute>
              <LoginPage />
            </PublicRoute>
          }
        />
        <Route
          path="/"
          element={
            <ProtectedRoute>
              <DashboardPage />
            </ProtectedRoute>
          }
        />
        <Route
          path="/keys"
          element={
            <ProtectedRoute>
              <KeysPage />
            </ProtectedRoute>
          }
        />
        <Route
          path="/keys/:keyId"
          element={
            <ProtectedRoute>
              <KeyDetailPage />
            </ProtectedRoute>
          }
        />
        <Route
          path="/keys/logs"
          element={
            <ProtectedRoute>
              <KeyUsageLogsPage />
            </ProtectedRoute>
          }
        />
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
    </BrowserRouter>
  )
}

export default AppRouter
