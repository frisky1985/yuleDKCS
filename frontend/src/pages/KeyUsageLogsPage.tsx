import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { useQuery } from '@tanstack/react-query';
import {
  Box, Typography, Paper, Table, TableBody, TableCell, TableContainer,
  TableHead, TableRow, TablePagination, Chip, IconButton, TextField,
  MenuItem, Select, FormControl, InputLabel, Grid, Button, DatePicker,
  LocalizationProvider, AdapterDateFns
} from '@mui/material';
import {
  ArrowBack, FilterList, Download, LocationOn,
  CheckCircle, Error as ErrorIcon, Schedule
} from '@mui/icons-material';
import { keysApi, type KeyUsageLog } from '../api/keys';
import { format, parseISO } from 'date-fns';
import { zhCN } from 'date-fns/locale';

const operationLabels: Record<string, string> = {
  unlock: '解锁',
  lock: '锁车',
  start_engine: '启动引擎',
  stop_engine: '停止引擎',
  open_trunk: '开后备箱',
  open_windows: '开窗',
  close_windows: '关窗',
  find_vehicle: '寻车',
  connect: 'BLE连接',
  disconnect: 'BLE断开',
};

export default function KeyUsageLogsPage() {
  const navigate = useNavigate();
  const [page, setPage] = useState(0);
  const [rowsPerPage, setRowsPerPage] = useState(25);
  const [startDate, setStartDate] = useState<Date | null>(null);
  const [endDate, setEndDate] = useState<Date | null>(null);
  const [operationFilter, setOperationFilter] = useState('');
  const [statusFilter, setStatusFilter] = useState('');

  const { data: logsData, isLoading } = useQuery({
    queryKey: ['allKeyLogs', page, rowsPerPage, startDate, endDate, operationFilter, statusFilter],
    queryFn: () => keysApi.getAllUsageLogs({
      page: page + 1,
      limit: rowsPerPage,
      start_date: startDate?.toISOString(),
      end_date: endDate?.toISOString(),
      operation: operationFilter || undefined,
    }),
  });

  const handleExport = () => {
    const logs = logsData?.logs || [];
    const csvContent = [
      ['时间', '操作', '状态', '设备', '位置', '失败原因'].join(','),
      ...logs.map(log => [
        format(new Date(log.timestamp), 'yyyy-MM-dd HH:mm:ss'),
        operationLabels[log.operation] || log.operation,
        log.status,
        log.device_info,
        log.location ? `${log.location.lat},${log.location.lng}` : '-',
        log.failure_reason || '',
      ].join(','))
    ].join('\n');

    const blob = new Blob(['\ufeff' + csvContent], { type: 'text/csv;charset=utf-8;' });
    const link = document.createElement('a');
    link.href = URL.createObjectURL(blob);
    link.download = `钥匙使用记录_${format(new Date(), 'yyyyMMdd')}.csv`;
    link.click();
  };

  const filteredLogs = (logsData?.logs || []).filter(log => {
    if (statusFilter && log.status !== statusFilter) return false;
    return true;
  });

  return (
    <Box sx={{ pb: 4 }}>
      {/* 顶部导航 */}
      <Box sx={{ p: 2, display: 'flex', alignItems: 'center', gap: 2 }}>
        <IconButton onClick={() => navigate('/keys')}>
          <ArrowBack />
        </IconButton>
        <Typography variant="h5" fontWeight="bold" sx={{ flex: 1 }}>
          钥匙使用记录
        </Typography>
        <Button
          variant="outlined"
          startIcon={<Download />}
          onClick={handleExport}
        >
          导出CSV
        </Button>
      </Box>

      {/* 筛选条件 */}
      <Paper sx={{ mx: 2, mb: 2, p: 2 }}>
        <Grid container spacing={2} alignItems="center">
          <Grid item xs={12} sm={3}>
            <TextField
              type="date"
              label="开始日期"
              value={startDate ? format(startDate, 'yyyy-MM-dd') : ''}
              onChange={(e) => setStartDate(e.target.value ? new Date(e.target.value) : null)}
              fullWidth
              InputLabelProps={{ shrink: true }}
            />
          </Grid>
          <Grid item xs={12} sm={3}>
            <TextField
              type="date"
              label="结束日期"
              value={endDate ? format(endDate, 'yyyy-MM-dd') : ''}
              onChange={(e) => setEndDate(e.target.value ? new Date(e.target.value) : null)}
              fullWidth
              InputLabelProps={{ shrink: true }}
            />
          </Grid>
          <Grid item xs={12} sm={3}>
            <FormControl fullWidth>
              <InputLabel>操作类型</InputLabel>
              <Select
                value={operationFilter}
                label="操作类型"
                onChange={(e) => setOperationFilter(e.target.value)}
              >
                <MenuItem value="">全部</MenuItem>
                {Object.entries(operationLabels).map(([key, label]) => (
                  <MenuItem key={key} value={key}>{label}</MenuItem>
                ))}
              </Select>
            </FormControl>
          </Grid>
          <Grid item xs={12} sm={3}>
            <FormControl fullWidth>
              <InputLabel>状态</InputLabel>
              <Select
                value={statusFilter}
                label="状态"
                onChange={(e) => setStatusFilter(e.target.value)}
              >
                <MenuItem value="">全部</MenuItem>
                <MenuItem value="success">成功</MenuItem>
                <MenuItem value="failure">失败</MenuItem>
                <MenuItem value="timeout">超时</MenuItem>
              </Select>
            </FormControl>
          </Grid>
        </Grid>
      </Paper>

      {/* 数据表格 */}
      <Paper sx={{ mx: 2 }}>
        <TableContainer>
          <Table>
            <TableHead>
              <TableRow>
                <TableCell>时间</TableCell>
                <TableCell>操作</TableCell>
                <TableCell>状态</TableCell>
                <TableCell>设备信息</TableCell>
                <TableCell>位置</TableCell>
                <TableCell>说明</TableCell>
              </TableRow>
            </TableHead>
            <TableBody>
              {isLoading ? (
                <TableRow>
                  <TableCell colSpan={6} align="center">加载中...</TableCell>
                </TableRow>
              ) : filteredLogs.length === 0 ? (
                <TableRow>
                  <TableCell colSpan={6} align="center">
                    暂无使用记录
                  </TableCell>
                </TableRow>
              ) : (
                filteredLogs.map((log) => (
                  <TableRow key={log.id} hover>
                    <TableCell>
                      {format(new Date(log.timestamp), 'MM-dd HH:mm:ss')}
                    </TableCell>
                    <TableCell>
                      {operationLabels[log.operation] || log.operation}
                    </TableCell>
                    <TableCell>
                      <Chip
                        size="small"
                        icon={
                          log.status === 'success' ? <CheckCircle /> :
                          log.status === 'failure' ? <ErrorIcon /> : <Schedule />
                        }
                        label={
                          log.status === 'success' ? '成功' :
                          log.status === 'failure' ? '失败' : '超时'
                        }
                        color={
                          log.status === 'success' ? 'success' :
                          log.status === 'failure' ? 'error' : 'warning'
                        }
                      />
                    </TableCell>
                    <TableCell>
                      <Typography variant="body2" noWrap sx={{ maxWidth: 200 }}>
                        {log.device_info}
                      </Typography>
                    </TableCell>
                    <TableCell>
                      {log.location ? (
                        <Box sx={{ display: 'flex', alignItems: 'center', gap: 0.5 }}>
                          <LocationOn fontSize="small" color="action" />
                          <Typography variant="caption">
                            {log.location.lat.toFixed(4)}, {log.location.lng.toFixed(4)}
                          </Typography>
                        </Box>
                      ) : (
                        '-'
                      )}
                    </TableCell>
                    <TableCell>
                      <Typography variant="caption" color="text.secondary">
                        {log.failure_reason || '-'}
                      </Typography>
                    </TableCell>
                  </TableRow>
                ))
              )}
            </TableBody>
          </Table>
        </TableContainer>
        <TablePagination
          component="div"
          count={logsData?.total || 0}
          page={page}
          onPageChange={(_, newPage) => setPage(newPage)}
          rowsPerPage={rowsPerPage}
          onRowsPerPageChange={(e) => {
            setRowsPerPage(parseInt(e.target.value, 10));
            setPage(0);
          }}
          rowsPerPageOptions={[10, 25, 50, 100]}
          labelRowsPerPage="每页行数:"
          labelDisplayedRows={({ from, to, count }) => `${from}-${to} / ${count}`}
        />
      </Paper>
    </Box>
  );
}
