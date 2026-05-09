import { Box, Container, AppBar, Toolbar, Typography, Button } from '@mui/material'
import { useAuthStore } from './store/auth'
import AppRouter from './router'

function App() {
  const { isAuthenticated, user, logout } = useAuthStore()

  return (
    <Box sx={{ flexGrow: 1 }}>
      <AppBar position="static">
        <Toolbar>
          <Typography variant="h6" component="div" sx={{ flexGrow: 1 }}>
            yuleDKCS
          </Typography>
          {isAuthenticated && user && (
            <>
              <Typography variant="body1" sx={{ mr: 2 }}>
                {user.username}
              </Typography>
              <Button color="inherit" onClick={logout}>
                退出
              </Button>
            </>
          )}
        </Toolbar>
      </AppBar>
      <Container maxWidth="lg" sx={{ mt: 4, mb: 4 }}>
        <AppRouter />
      </Container>
    </Box>
  )
}

export default App
