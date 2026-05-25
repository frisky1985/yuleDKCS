# Frontend Agent Instructions

## Commands

```bash
# Development
npm install
npm run dev              # Dev server (http://localhost:5173)

# Testing
npm run test             # Run tests
npm run test:watch       # Watch mode
npm run test:coverage    # With coverage report

# Quality
npm run lint             # ESLint
npm run lint:fix         # Auto-fix
npm run type-check       # TypeScript check
npm run format           # Prettier

# Build
npm run build            # Production build
npm run preview          # Preview production build
```

## Structure

```
frontend/
├── src/
│   ├── api/            # API client functions
│   ├── components/     # Reusable UI components
│   ├── pages/          # Page components (routes)
│   ├── hooks/          # Custom React hooks
│   ├── stores/         # Zustand/Jotai stores
│   ├── types/          # TypeScript types
│   ├── utils/          # Utility functions
│   └── styles/         # Global styles
├── public/             # Static assets
└── index.html
```

## Patterns

### API Client
```typescript
// src/api/vehicles.ts
import { apiClient } from './client';

export async function getVehicleStatus(id: string): Promise<VehicleStatus> {
  const response = await apiClient.get(`/vehicles/${id}/status`);
  return response.data;
}
```

### Component
```typescript
// Use functional components with hooks
import { useQuery } from '@tanstack/react-query';

export function VehicleCard({ vehicleId }: { vehicleId: string }) {
  const { data, isLoading, error } = useQuery({
    queryKey: ['vehicle', vehicleId],
    queryFn: () => getVehicleStatus(vehicleId),
  });
  
  if (isLoading) return <Skeleton />;
  if (error) return <ErrorMessage error={error} />;
  
  return <div>{/* Render vehicle */}</div>;
}
```

### Custom Hook
```typescript
// src/hooks/useVehicle.ts
export function useVehicle(id: string) {
  return useQuery({
    queryKey: ['vehicle', id],
    queryFn: () => getVehicle(id),
    staleTime: 30000,
  });
}
```

## Testing

- Framework: Vitest + React Testing Library
- Component tests: `*.test.tsx` next to source
- Mock API calls with MSW (Mock Service Worker)
- Target: ≥70% coverage

## State Management

- Server state: TanStack Query (React Query)
- Client state: Zustand (simple) or Jotai (atoms)
- Avoid Redux unless complex state interactions

## Styling

- CSS Modules or Tailwind CSS
- Theme in `src/styles/theme.ts`
- Responsive design: Mobile-first

## API Integration

- Base URL from env var: `VITE_API_URL`
- Axios instance with interceptors for auth
- Error handling with toast notifications
