// 通用类型定义

export interface PaginationParams {
  page: number
  pageSize: number
}

export interface PaginatedResponse<T> {
  items: T[]
  total: number
  page: number
  pageSize: number
  totalPages: number
}

export interface CardProduct {
  id: string
  name: string
  description: string
  price: number
  faceValue: number
  category: string
  stock: number
  status: 'active' | 'inactive'
  createdAt: string
  updatedAt: string
}

export interface Order {
  id: string
  userId: string
  productId: string
  quantity: number
  totalAmount: number
  status: 'pending' | 'processing' | 'completed' | 'failed' | 'refunded'
  cardNumbers?: string[]
  createdAt: string
  updatedAt: string
}

export interface RechargeTask {
  id: string
  orderId: string
  accountNumber: string
  amount: number
  status: 'pending' | 'processing' | 'success' | 'failed'
  retryCount: number
  resultMessage?: string
  createdAt: string
  updatedAt: string
}
