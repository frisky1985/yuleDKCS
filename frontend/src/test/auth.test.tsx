import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, waitFor } from '@testing-library/react'
import { QueryClient, QueryClientProvider } from '@tanstack/react-query'
import { BrowserRouter } from 'react-router-dom'
import LoginPage from '../pages/auth/LoginPage'
import { useAuthStore } from '../store/auth'

// 模拟 API
vi.mock('../api/client', () => ({
  api: {
    post: vi.fn(),
  },
}))

const createWrapper = () => {
  const queryClient = new QueryClient({
    defaultOptions: {
      queries: {
        retry: false,
      },
    },
  })
  
  return ({ children }: { children: React.ReactNode }) => (
    <QueryClientProvider client={queryClient}>
      <BrowserRouter>{children}</BrowserRouter>
    </QueryClientProvider>
  )
}

describe('LoginPage', () => {
  beforeEach(() => {
    useAuthStore.setState({ user: null, token: null, isAuthenticated: false })
  })

  it('应该渲染登录表单', () => {
    render(<LoginPage />, { wrapper: createWrapper() })
    
    expect(screen.getByText('登录')).toBeInTheDocument()
    expect(screen.getByLabelText(/手机号或邮箱/i)).toBeInTheDocument()
    expect(screen.getByLabelText(/密码/i)).toBeInTheDocument()
    expect(screen.getByRole('button', { name: /登录/i })).toBeInTheDocument()
  })

  it('应该显示验证错误当提交空表单', async () => {
    render(<LoginPage />, { wrapper: createWrapper() })
    
    const submitButton = screen.getByRole('button', { name: /登录/i })
    fireEvent.click(submitButton)
    
    await waitFor(() => {
      expect(screen.getByText(/请输入手机号或邮箱/i)).toBeInTheDocument()
    })
  })

  it('应该能够切换密码显示', () => {
    render(<LoginPage />, { wrapper: createWrapper() })
    
    const passwordInput = screen.getByLabelText(/密码/i)
    expect(passwordInput).toHaveAttribute('type', 'password')
    
    const toggleButton = screen.getByRole('button', { name: /显示密码/i })
    fireEvent.click(toggleButton)
    
    expect(passwordInput).toHaveAttribute('type', 'text')
  })
})
